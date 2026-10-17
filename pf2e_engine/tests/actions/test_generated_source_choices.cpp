#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/generated_source.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/inventory/generated_source_choices.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

class TGeneratedSourceChoicesTest : public TBattleFixture {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedSourceChoicesRules(*rules);
        StartBattle(std::move(rules), 4, "generated_source_choices.");
    }

    TPlayer& Add(std::string name, bool trained, int level = 1, bool armed = false)
    {
        TIdCollection<TFeatureSelection> features;
        if (trained) {
            features.Add(TFeatureSelection("generated_source_choices.EquipmentTraining"));
        }
        TIdCollection<TWeapon> weapons;
        TIdCollection<TWeapon> natural;
        if (armed) {
            weapons.Add(CreateChoiceBlade());
            natural.Add(CreateChoiceFang());
        }
        const int id = static_cast<int>(creatures_.size());
        return Join(TCreature::Create({
            .Level = level,
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 100}), .Current = 100,
            }),
            .Weapons = std::move(weapons),
            .NaturalWeapons = std::move(natural),
            .Features = std::move(features),
            .Actions = TResource(3),
            .Hands = TResource(2),
        }), id, {id, 0}, std::move(name));
    }

    TPlayer& Actor(int level = 1, bool armed = false)
    {
        return Add("Actor", true, level, armed);
    }

    TPlayer& Target()
    {
        return Add("Target", false);
    }

    static std::vector<TGeneratedSourceCandidate<TWeapon>> Candidates(TPlayer& actor,
                                                                     bool natural = false)
    {
        return SourceCandidates<TCreature, TWeapon>(BindActionEntity<TCreature>(actor),
            TAbilitySourcePath{}.Field(natural ? "natural_weapons" : "weapons"),
            [natural](const TCreature& creature) -> const TIdCollection<TWeapon>& {
                return natural ? creature.NaturalWeapons() : creature.Weapons();
            }, [](const TWeapon& weapon) { return weapon.Name(); });
    }

    static void SetDie(TTransformator& editor, TPlayer& actor, int amount)
    {
        auto execution = editor.ExecutionContext();
        auto weapon = execution.Project(actor.GetCreature()->Weapons(), [](TIdCollection<TWeapon>& values) {
            return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
        });
        auto die = execution.Project(weapon, [](TWeapon& value) { return value.BaseDiceSize(); });
        execution.AssignGuarded(die, amount, weapon, "base_dice_size");
    }

    static void RemoveWeapon(TTransformator& editor, TPlayer& actor)
    {
        auto weapons = actor.GetCreature()->Weapons().Get();
        weapons.Remove(TId<TWeapon>{0});
        editor.ExecutionContext().AssignGuarded(actor.GetCreature()->Weapons(), std::move(weapons),
            TGuarded<TCreature>(*actor.GetCreature()), "weapons");
    }
};

TEST_F(TGeneratedSourceChoicesTest, CandidateUnionUsesStableOwnerPathsWithoutRequiringWeaponGrants)
{
    auto& actor = Actor(1, true);
    auto& other = Add("Other", true, 1, true);
    const auto before = Snapshot();
    const auto candidates = MergeSourceCandidates<TWeapon>({Candidates(actor), Candidates(actor, true),
                                                            Candidates(actor), Candidates(other)});
    ASSERT_EQ(candidates.size(), 3u);
    EXPECT_EQ(candidates[0].Key().Path, TAbilitySourcePath{}.Field("weapons").Entry(0));
    EXPECT_EQ(candidates[1].Key().Path, TAbilitySourcePath{}.Field("natural_weapons").Entry(0));
    EXPECT_NE(candidates[0].Key(), candidates[2].Key());
    EXPECT_EQ(candidates[0].Read().Features().Size(), 0u);
    EXPECT_EQ(candidates[1].Read().BaseDiceSize(), 8);
    const auto context = Context(actor.GetId());
    int resumed = 0;
    io_.ExpectName(0, {}, "Blade (2)");
    const auto selected = SelectActionSource<TWeapon>(context, candidates, {},
        [&](auto) { ++resumed; });
    EXPECT_EQ(selected.Key(), candidates[1].Key());
    EXPECT_EQ(selected.Read().BaseDiceSize(), 8);
    EXPECT_EQ(io_.Candidates("source"), (std::vector<std::string>{"Blade (1)", "Blade (2)", "Blade (3)"}));
    EXPECT_EQ(resumed, 0);
    ExpectSame(before, Snapshot(), "immediate source choice");
}

TEST_F(TGeneratedSourceChoicesTest, ReceiveRechecksCurrentCandidateAndThenOwnsItsSnapshot)
{
    auto& actor = Actor(1, true);
    const auto context = Context(actor.GetId());
    const auto candidates = Candidates(actor);
    std::optional<TGeneratedSourceBinding<TWeapon>> selected;
    io_.ExpectName(0, {}, "Blade", true);
    const auto point = Pause([&] {
        SelectActionSource<TWeapon>(context, candidates,
            [](const auto& candidate) { return candidate.Read().BaseDiceSize() < 10; },
            [&](auto value) { selected.emplace(std::move(value)); });
    });
    TTransformator editor(io_);
    const auto initial = editor.CurrentState();
    SetDie(editor, actor, 12);
    EXPECT_EQ(candidates[0].Read().BaseDiceSize(), 12);
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_FALSE(selected);
    editor.Undo(initial);
    SetDie(editor, actor, 6);
    point.Resume();
    ASSERT_TRUE(selected);
    EXPECT_EQ(selected->Read().BaseDiceSize(), 6);
    SetDie(editor, actor, 9);
    EXPECT_EQ(candidates[0].Read().BaseDiceSize(), 9);
    EXPECT_EQ(selected->Read().BaseDiceSize(), 6);
    EXPECT_NO_THROW(selected->Validate(context));
}

TEST_F(TGeneratedSourceChoicesTest, MissingExactEntryCannotFallBackToAnotherProvider)
{
    auto& actor = Actor(1, true);
    const auto context = Context(actor.GetId());
    const auto candidate = Candidates(actor).front();
    const auto selected = candidate.Snapshot(context);
    TTransformator editor(io_);
    const auto initial = editor.CurrentState();
    RemoveWeapon(editor, actor);
    EXPECT_THROW(candidate.Read(), TAbilityUnavailable);
    EXPECT_THROW(candidate.Validate(context), TAbilityUnavailable);
    EXPECT_THROW(selected.Read(), TAbilityUnavailable);
    EXPECT_EQ(Candidates(actor, true).size(), 1u);
    editor.Undo(initial);
    EXPECT_EQ(candidate.Read().BaseDiceSize(), 4);
    EXPECT_EQ(selected.Read().BaseDiceSize(), 4);
}

TEST_F(TGeneratedSourceChoicesTest, InputForkValidatesCurrentWorldWithoutCyclesOrParentLeakage)
{
    auto& actor = Actor(1, true);
    const auto parent = Context(actor.GetId());
    const auto selected = Candidates(actor).front().Snapshot(parent);
    auto child = ForkActionInputs(parent, {selected.InputValidator()});
    EXPECT_EQ(child->transformator, parent->transformator);
    EXPECT_EQ(child->actor, parent->actor);
    EXPECT_FALSE(parent->input_validators);
    EXPECT_NO_THROW(ValidateActionActor(child));
    child->battle = nullptr;
    EXPECT_THROW(ValidateActionActor(child), TAbilityUnavailable);
    child->battle = battle_.get();
    EXPECT_NO_THROW(ValidateActionActor(child));
    TTransformator editor(io_);
    const auto initial = editor.CurrentState();
    RemoveWeapon(editor, actor);
    EXPECT_THROW(ValidateActionActor(child), TAbilityUnavailable);
    EXPECT_NO_THROW(ValidateActionActor(parent));
    editor.Undo(initial);
    EXPECT_NO_THROW(ValidateActionActor(child));
    std::weak_ptr<TActionContext> weak = child;
    child.reset();
    EXPECT_TRUE(weak.expired());
    EXPECT_THROW(ForkActionInputs(parent, {FActionInputValidator{}}), std::invalid_argument);
}

TEST_F(TGeneratedSourceChoicesTest, InputForkComposesDependenciesAndRejectsForeignOwners)
{
    auto& actor = Actor(1, true);
    const auto parent = Context(actor.GetId());
    const auto first = Candidates(actor).front().Snapshot(parent);
    const auto second = Candidates(actor, true).front().Snapshot(parent);
    const auto child = ForkActionInputs(parent, {first.InputValidator()});
    const auto grandchild = ForkActionInputs(child, {second.InputValidator()});
    ASSERT_TRUE(grandchild->input_validators);
    EXPECT_EQ(grandchild->input_validators->size(), 2u);
    TTransformator editor(io_);
    const auto initial = editor.CurrentState();
    RemoveWeapon(editor, actor);
    EXPECT_THROW(ValidateAbilitySource(grandchild), TAbilityUnavailable);
    editor.Undo(initial);
    EXPECT_NO_THROW(ValidateAbilitySource(grandchild));
    auto foreign_creature = TCreature::Create({});
    TPlayer foreign(&foreign_creature, TPlayerTeam{0}, TPlayerId{9}, "Foreign", "");
    EXPECT_THROW((SourceCandidates<TCreature, TWeapon>(parent,
        BindActionEntity<TCreature>(foreign), TAbilitySourcePath{}.Field("weapons"),
        [](const TCreature& value) -> const TIdCollection<TWeapon>& { return value.Weapons(); })),
        TAbilityUnavailable);
}

TEST_F(TGeneratedSourceChoicesTest, CompiledChildResumeChecksItsOwnCurrentContextWorld)
{
    auto& actor = Actor(1, true);
    auto& target = Target();
    const auto parent = Context(actor.GetId());
    const auto selected = Candidates(actor).front().Snapshot(parent);
    std::weak_ptr<TActionContext> observed;
    const auto child = ForkActionInputs(parent, {selected.InputValidator(),
        [&observed](const std::shared_ptr<TActionContext>& current) { observed = current; }});
    int resumed = 0;
    std::function<void(int)> on_resume = [&](int amount) {
        EXPECT_EQ(amount, 4);
        ++resumed;
    };
    io_.ExpectValue(0, 0, true);
    const auto point = Pause([&] {
        RunGeneratedSourceChoicesChosenWeaponWrapper(child, selected.Read(),
            BindActionEntity<TCreature>(target), on_resume);
    });
    const auto active = observed.lock();
    ASSERT_TRUE(active);
    EXPECT_NE(active, parent);
    active->battle = nullptr;
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(resumed, 0);
    EXPECT_EQ(parent->battle, battle_.get());
    active->battle = battle_.get();
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 96);
    EXPECT_EQ(resumed, 1);
}

TEST_F(TGeneratedSourceChoicesTest, SourceChoicePreflightHidesTheActionWithoutEquipment)
{
    Actor();
    Target();
    EXPECT_FALSE(Available("ChooseEquipment"));
}

TEST_F(TGeneratedSourceChoicesTest, CompiledSourceUnionSelectsANaturalWeaponWithoutAGrant)
{
    Actor(1, true);
    Target();
    Act("ChooseEquipment", 0, {.Choose = {"Blade (2)", "Target", TPick::Value(1)},
        .Changes = {{"Target.hitpoints.current", 91}, {"Actor.hands.count", 9}, {"Actor.actions.count", 2}}});
    EXPECT_EQ(io_.Candidates("source"), (std::vector<std::string>{"Blade (1)", "Blade (2)"}));
    EXPECT_EQ(Read(0).NaturalWeapons().Find(TId<TWeapon>{0})->Features().Size(), 0u);
}

TEST_F(TGeneratedSourceChoicesTest, CompiledChildKeepsTheSnapshotTakenWhenTheSourceWasChosen)
{
    auto& actor = Actor(1, true);
    Target();
    io_.ExpectName(0, {}, "Blade (1)", true);
    const auto source_point = Pause([&] { Execute("ChooseEquipment", actor.GetId()); });
    TTransformator editor(io_);
    SetDie(editor, actor, 6);
    io_.ExpectName(0, {}, "Target");
    io_.ExpectValue(0, 1, true);
    const auto child_point = Pause([&] { source_point.Resume(); });
    SetDie(editor, actor, 12);
    child_point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 93);
    EXPECT_EQ(Read(0).Hands().Count(), 7);
}

TEST_F(TGeneratedSourceChoicesTest, CompletedChildInputConstraintsDoNotLeakPastTheParentScope)
{
    auto& actor = Actor(1, true);
    io_.ExpectName(0, {}, "Blade");
    io_.ExpectValue(0, 1, true);
    const auto point = Pause([&] { Execute("ScopedEquipment", actor.GetId()); });
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 4);
    TTransformator editor(io_);
    RemoveWeapon(editor, actor);
    point.Resume();
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 5);
}

TEST_F(TGeneratedSourceChoicesTest, RepeatedImmediateSourceChoicesAndCallsKeepStackDepthBounded)
{
    auto& actor = Actor(2048, true);
    for (int index = 0; index < 2048; ++index) {
        io_.ExpectName(0, {}, "Blade");
    }
    EXPECT_EQ(Execute("RepeatedEquipment", actor.GetId()), EAbilityExecutionResult::Applied);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 8192);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(io_.Requests(), 2048u);
    EXPECT_EQ(io_.MaxDepth(), 1);
}

}

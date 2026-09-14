#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/generated_source.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/inventory/generated_source_choices.h>

#include "../test_lib/mock_dice_roller.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <any>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

class TSourceChoiceIo final : public IInteractionSystem {
public:
    template <class T>
    void ExpectValue(T value, bool suspend = false)
    {
        expected_.push_back({[value](const TAlternative& alternative) {
            const auto* actual = std::any_cast<T>(&alternative.value);
            return actual != nullptr && *actual == value;
        }, suspend});
    }

    void ExpectTarget(bool suspend = false)
    {
        expected_.push_back({[](const TAlternative& alternative) {
            return alternative.name == "Target";
        }, suspend});
    }

    void ExpectSource(std::string name, bool suspend = false)
    {
        expected_.push_back({[name = std::move(name)](const TAlternative& alternative) {
            return alternative.name == name;
        }, suspend});
    }

    void RequestChoice(int player_id, TAlternatives alternatives, TState state,
                       std::function<void(size_t)> receive) override
    {
        if (expected_.empty()) {
            throw std::logic_error("unexpected source choice");
        }
        if (alternatives.Kind() == "source") {
            source_names_.clear();
            for (size_t index = 0; index < alternatives.Size(); ++index) {
                source_names_.push_back(alternatives[index].name);
            }
        }
        const bool suspend = expected_.front().Suspend;
        const auto index = ChooseAlternativeIndex(player_id, alternatives);
        ++requests_;
        if (suspend) {
            throw TSavepointStackUnwind(state,
                [receive = std::move(receive), index]() { receive(index); });
        }
        ++depth_;
        max_depth_ = std::max(max_depth_, depth_);
        try {
            receive(index);
        } catch (...) {
            --depth_;
            throw;
        }
        --depth_;
    }

    void HandleReactionTrigger(const TTriggerContext&, const TState&) override {}
    std::ostream& GameLog() override { return log_; }
    std::ostream& DevLog() override { return log_; }
    void Verify() const { EXPECT_TRUE(expected_.empty()); }
    size_t Requests() const { return requests_; }
    int MaxDepth() const { return max_depth_; }
    const std::vector<std::string>& SourceNames() const { return source_names_; }

protected:
    size_t ChooseAlternativeIndex(int player_id, const TAlternatives& alternatives) override
    {
        EXPECT_EQ(player_id, 0);
        if (expected_.empty()) {
            throw std::logic_error("unexpected source alternative");
        }
        auto expected = std::move(expected_.front());
        expected_.pop_front();
        for (size_t index = 0; index < alternatives.Size(); ++index) {
            if (expected.Matches(alternatives[index])) {
                return index;
            }
        }
        throw std::logic_error("expected source alternative is absent");
    }

private:
    struct TExpected {
        std::function<bool(const TAlternative&)> Matches;
        bool Suspend;
    };

    std::deque<TExpected> expected_;
    size_t requests_ = 0;
    int depth_ = 0;
    int max_depth_ = 0;
    std::ostringstream log_;
    std::vector<std::string> source_names_;
};

class TGeneratedSourceChoicesTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedSourceChoicesRules(*rules);
        RegisterGeneratedSourceChoicesRules(*rules);
        nlohmann::json map = {{"x_size", 4}, {"y_size", 4}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, std::move(rules));
        journal_ = std::make_unique<TTransformator>(io_);
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
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
        creatures_.push_back(TCreature::Create({
            .Level = level,
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 100}), .Current = 100,
            }),
            .Weapons = std::move(weapons),
            .NaturalWeapons = std::move(natural),
            .Features = std::move(features),
            .Actions = TResource(3),
            .Hands = TResource(2),
        }));
        rng_.ExpectCall(20, 10);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id}, TPlayerId{id},
                                   std::move(name), ""), {id, 0});
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    TPlayer& Actor(int level = 1, bool armed = false)
    {
        return Add("Actor", true, level, armed);
    }

    TPlayer& Target()
    {
        return Add("Target", false);
    }

    std::shared_ptr<TActionContext> Context(TPlayer* actor = nullptr)
    {
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = journal_.get(),
            .io_system = &io_, .scheduler = nullptr, .effect_manager = nullptr,
        });
        if (actor != nullptr) {
            context->game_object_registry = std::make_shared<TGameObjectRegistry>();
            context->game_object_registry->Add(
                TGameObjectIdManager::Instance().Register("self"), actor);
        }
        return context;
    }

    EAbilityExecutionResult Execute(TPlayer& actor, const std::string& name)
    {
        const auto options = battle_->AvailableActions(actor);
        const auto found = std::ranges::find_if(options, [&](const TAbilityOption& option) {
            return option.DefinitionId == "generated_source_choices." + name;
        });
        if (found == options.end()) {
            throw std::logic_error("missing source choice action: " + name);
        }
        const auto& rules = battle_->AbilityRules();
        last_context_ = Context();
        return ExecuteAbility(*found, last_context_, actor,
            CollectAbilities(*actor.GetCreature(), rules.Catalog), rules.Executions,
            {.ValidateSource = [this, &actor, definition = found->DefinitionId,
                                source = found->Source]() {
                const auto fresh = CollectAbilities(*actor.GetCreature(),
                                                     battle_->AbilityRules().Catalog);
                return std::ranges::any_of(fresh.Grants(), [&](const TGrantedAbility& grant) {
                    return grant.DefinitionId == definition && grant.Source == source;
                });
            }});
    }

    static TSavepointStackUnwind Suspend(const std::function<void()>& work)
    {
        try {
            work();
        } catch (const TSavepointStackUnwind& point) {
            return point;
        }
        throw std::logic_error("source choice did not suspend");
    }

    TAstNode Snapshot() const
    {
        TAstContext context;
        auto node = TAstNode::MakeObject("generated_source_choice_execution");
        AddOwnedObject(node, "journal", *journal_, context);
        AddOwnedObject(node, "battle", *battle_, context);
        return node;
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

    TMockRng rng_;
    TSourceChoiceIo io_;
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> journal_;
    std::shared_ptr<TActionContext> last_context_;
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
    EXPECT_EQ(Snapshot(), before);
    const auto context = Context(&actor);
    int resumed = 0;
    io_.ExpectSource("Blade (2)");
    const auto selected = SelectActionSource<TWeapon>(context, candidates, {},
        [&](auto) { ++resumed; });
    EXPECT_EQ(selected.Key(), candidates[1].Key());
    EXPECT_EQ(selected.Read().BaseDiceSize(), 8);
    EXPECT_EQ(io_.SourceNames(), (std::vector<std::string>{"Blade (1)", "Blade (2)", "Blade (3)"}));
    EXPECT_EQ(resumed, 0);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedSourceChoicesTest, ReceiveRechecksCurrentCandidateAndThenOwnsItsSnapshot)
{
    auto& actor = Actor(1, true);
    const auto context = Context(&actor);
    const auto candidates = Candidates(actor);
    std::optional<TGeneratedSourceBinding<TWeapon>> selected;
    io_.ExpectSource("Blade", true);
    const auto point = Suspend([&] {
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
    editor.Undo(initial);
}

TEST_F(TGeneratedSourceChoicesTest, MissingExactEntryCannotFallBackToAnotherProvider)
{
    auto& actor = Actor(1, true);
    const auto context = Context(&actor);
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
    const auto parent = Context(&actor);
    const auto selected = Candidates(actor).front().Snapshot(parent);
    auto child = ForkActionInputs(parent, {selected.InputValidator()});
    EXPECT_EQ(child->transformator, parent->transformator);
    EXPECT_EQ(child->game_object_registry, parent->game_object_registry);
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
    const auto parent = Context(&actor);
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
    const auto parent = Context(&actor);
    const auto selected = Candidates(actor).front().Snapshot(parent);
    std::weak_ptr<TActionContext> observed;
    const auto child = ForkActionInputs(parent, {selected.InputValidator(),
        [&observed](const std::shared_ptr<TActionContext>& current) { observed = current; }});
    int resumed = 0;
    std::function<void(int)> on_resume = [&](int amount) {
        EXPECT_EQ(amount, 4);
        ++resumed;
    };
    io_.ExpectValue(0, true);
    const auto point = Suspend([&] {
        RunGeneratedSourceChoicesChosenWeaponWrapper(child, selected.Read(),
            BindActionEntity<TCreature>(target), on_resume);
    });
    const auto active = observed.lock();
    ASSERT_TRUE(active);
    EXPECT_NE(active, parent);
    active->battle = nullptr;
    const auto paused = Snapshot();
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Snapshot(), paused);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(resumed, 0);
    EXPECT_EQ(parent->battle, battle_.get());
    active->battle = battle_.get();
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 96);
    EXPECT_EQ(resumed, 1);
    const auto completed = Snapshot();
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(Snapshot(), completed);
    EXPECT_EQ(resumed, 2);
}

TEST_F(TGeneratedSourceChoicesTest, SourceChoicePreflightIsPureAndHidesEmptyEquipmentBeforeCost)
{
    auto& actor = Actor();
    Target();
    const auto before = Snapshot();
    const auto options = battle_->AvailableActions(actor);
    EXPECT_FALSE(std::ranges::any_of(options, [](const auto& option) {
        return option.DefinitionId == "generated_source_choices.ChooseEquipment";
    }));
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    EXPECT_EQ(io_.Requests(), 0u);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedSourceChoicesTest, CompiledSourceUnionSelectsNaturalWeaponWithoutGrantingTheAction)
{
    auto& actor = Actor(1, true);
    auto& target = Target();
    io_.ExpectSource("Blade (2)");
    io_.ExpectTarget();
    io_.ExpectValue(1);
    EXPECT_EQ(Execute(actor, "ChooseEquipment"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(io_.SourceNames(), (std::vector<std::string>{"Blade (1)", "Blade (2)"}));
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 91);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 9);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(actor.GetCreature()->NaturalWeapons()->Find(TId<TWeapon>{0})->Features().Size(), 0u);
}

TEST_F(TGeneratedSourceChoicesTest, CompiledChildRetainsSelectedSnapshotAcrossRepeatedSuspension)
{
    auto& actor = Actor(1, true);
    auto& target = Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectSource("Blade (1)", true);
    const auto source_point = Suspend([&] { Execute(actor, "ChooseEquipment"); });
    TTransformator editor(io_);
    const auto edit_start = editor.CurrentState();
    SetDie(editor, actor, 6);
    io_.ExpectTarget();
    io_.ExpectValue(1, true);
    const auto child_point = Suspend([&] { source_point.Resume(); });
    SetDie(editor, actor, 12);
    child_point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 93);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 7);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    const auto completed = Snapshot();
    child_point.Revert(*journal_);
    child_point.Resume();
    EXPECT_EQ(Snapshot(), completed);
    journal_->Undo(initial);
    editor.Undo(edit_start);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedSourceChoicesTest, CompiledChildRejectsSourceRemovalAndRestoredSourceCanReplay)
{
    auto& actor = Actor(1, true);
    auto& target = Target();
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectSource("Blade (1)");
    io_.ExpectTarget();
    io_.ExpectValue(0, true);
    const auto point = Suspend([&] { Execute(actor, "ChooseEquipment"); });
    TTransformator editor(io_);
    const auto edit_start = editor.CurrentState();
    RemoveWeapon(editor, actor);
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    editor.Undo(edit_start);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 96);
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 96);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedSourceChoicesTest, CompletedChildInputConstraintsDoNotLeakPastTheParentScope)
{
    auto& actor = Actor(1, true);
    io_.ExpectSource("Blade");
    io_.ExpectValue(1, true);
    const auto point = Suspend([&] { Execute(actor, "ScopedEquipment"); });
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 4);
    TTransformator editor(io_);
    const auto initial = editor.CurrentState();
    RemoveWeapon(editor, actor);
    point.Resume();
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 5);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 5);
    editor.Undo(initial);
}

TEST_F(TGeneratedSourceChoicesTest, RepeatedImmediateSourceChoicesAndCallsKeepStackDepthBounded)
{
    auto& actor = Actor(2048, true);
    for (int index = 0; index < 2048; ++index) {
        io_.ExpectSource("Blade");
    }
    EXPECT_EQ(Execute(actor, "RepeatedEquipment"), EAbilityExecutionResult::Applied);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 8192);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(io_.Requests(), 2048u);
    EXPECT_EQ(io_.MaxDepth(), 1);
}

}

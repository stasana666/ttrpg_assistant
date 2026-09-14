#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/inventory/generated_sources.h>

#include "../test_lib/mock_dice_roller.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class TSourceChoiceIo final : public IInteractionSystem {
public:
    void ExpectTarget(std::string name, bool suspend = false)
    {
        expected_.push_back({std::move(name), suspend});
    }

    void RequestChoice(int player_id, TAlternatives alternatives, TState state,
                       std::function<void(size_t)> on_choice) override
    {
        if (expected_.empty()) {
            throw std::logic_error("unexpected source action choice");
        }
        const bool suspend = expected_.front().Suspend;
        const auto index = ChooseAlternativeIndex(player_id, alternatives);
        if (suspend) {
            throw TSavepointStackUnwind(state,
                [on_choice = std::move(on_choice), index]() { on_choice(index); });
        }
        on_choice(index);
    }

    void HandleReactionTrigger(const TTriggerContext&, const TState&) override {}
    std::ostream& GameLog() override { return log_; }
    std::ostream& DevLog() override { return log_; }
    void Verify() const { EXPECT_TRUE(expected_.empty()); }
    const std::vector<std::string>& Candidates() const { return candidates_; }

protected:
    size_t ChooseAlternativeIndex(int player_id, const TAlternatives& alternatives) override
    {
        EXPECT_EQ(player_id, 0);
        if (expected_.empty()) {
            throw std::logic_error("unexpected source action alternative");
        }
        const auto expected = expected_.front();
        expected_.pop_front();
        candidates_.clear();
        std::optional<size_t> result;
        for (size_t index = 0; index < alternatives.Size(); ++index) {
            candidates_.push_back(alternatives[index].name);
            if (alternatives[index].name == expected.Name) {
                result = index;
            }
        }
        if (!result) {
            throw std::logic_error("expected target is absent: " + expected.Name);
        }
        return *result;
    }

private:
    struct TExpected {
        std::string Name;
        bool Suspend;
    };

    std::deque<TExpected> expected_;
    std::vector<std::string> candidates_;
    std::ostringstream log_;
};

TAbilitySourcePath WeaponSource(std::size_t id)
{
    return TAbilitySourcePath{}.Field("weapons").Entry(id).Field("features").Entry(0);
}

class TGeneratedSourcesTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedSourcesRules(*rules);
        RegisterGeneratedSourcesRules(*rules);
        nlohmann::json map = {{"x_size", 8}, {"y_size", 8}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, std::move(rules));
        journal_ = std::make_unique<TTransformator>(io_);
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
    }

    TPlayer& Add(std::string name, TCreature creature)
    {
        const int id = static_cast<int>(creatures_.size());
        creatures_.push_back(std::move(creature));
        rng_.ExpectCall(20, 10);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id == 0 ? 0 : 1},
                                   TPlayerId{id}, std::move(name), ""), {id, 0});
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    TPlayer& AddTarget(std::string name)
    {
        return Add(std::move(name), TCreature::Create({
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 100}),
                .Current = 100,
            }),
        }));
    }

    TAbilityOption Option(TPlayer& actor, const TAbilitySourcePath& source) const
    {
        const auto options = battle_->AvailableActions(actor);
        const auto found = std::ranges::find_if(options, [&](const TAbilityOption& option) {
            return option.DefinitionId == "generated_sources.WeaponProbe"
                && option.Source == source;
        });
        if (found == options.end()) {
            throw std::logic_error("missing source-bound probe option");
        }
        return *found;
    }

    EAbilityExecutionResult ExecuteJournal(TPlayer& actor, const TAbilityOption& option)
    {
        const auto& rules = battle_->AbilityRules();
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(),
            .dice_roller = &rng_,
            .transformator = journal_.get(),
            .io_system = &io_,
            .scheduler = nullptr,
            .effect_manager = nullptr,
        });
        return ExecuteAbility(option, std::move(context), actor,
            CollectAbilities(*actor.GetCreature(), rules.Catalog), rules.Executions,
            {.ValidateSource = [this, &actor, definition = option.DefinitionId,
                                source = option.Source]() {
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
        throw std::logic_error("source action did not suspend");
    }

    TAstNode Snapshot() const
    {
        TAstContext context;
        auto node = TAstNode::MakeObject("generated_source_execution");
        AddOwnedObject(node, "journal", *journal_, context);
        AddOwnedObject(node, "battle", *battle_, context);
        return node;
    }

    TMockRng rng_;
    TSourceChoiceIo io_;
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> journal_;
};

TEST_F(TGeneratedSourcesTest, NamedValuesPreserveTraitPayloadsAndTypedQueryArguments)
{
    const auto light = CreateProbeLight();
    const auto heavy = CreateProbeHeavy();
    const auto profile = TWeaponProbeProfile::Create({});
    EXPECT_EQ(light.Name(), heavy.Name());
    EXPECT_EQ(light.BaseDiceSize(), 4);
    EXPECT_EQ(heavy.BaseDiceSize(), 12);
    EXPECT_TRUE(light.Traits().Has(EWeaponTraitKind::Agile));
    EXPECT_FALSE(heavy.Traits().Has(EWeaponTraitKind::Agile));
    ASSERT_NE(light.Traits().Get<TWeaponTraitThrown>(), nullptr);
    EXPECT_EQ(light.Traits().Get<TWeaponTraitThrown>()->RangeFeet, 2);
    ASSERT_NE(heavy.Traits().Get<TWeaponTraitFatal>(), nullptr);
    EXPECT_EQ(heavy.Traits().Get<TWeaponTraitFatal>()->Die, EDieSize::D10);
    EXPECT_EQ(profile.Bonus(light), 5);
    EXPECT_EQ(profile.Bonus(heavy), 8);
    EXPECT_FALSE(profile.IsFatalD10(light));
    EXPECT_TRUE(profile.IsFatalD10(heavy));
    EXPECT_EQ(profile.Bonus(TWeapon::Create({
        .DamageType = EDamageType::Slashing,
        .Category = EWeaponCategory::Unarmed,
    })), 0);
}

TEST_F(TGeneratedSourcesTest, NamedLoadoutsCopyValuesAndCollectionQueriesReadCurrentContents)
{
    auto first = CreateProbeLoadout();
    const auto second = CreateProbeLoadout();
    const auto before = first.ToJson();
    EXPECT_EQ(first.MaximumReach(), 3);
    EXPECT_EQ(first.TotalDiceSize(), 20);
    EXPECT_EQ(first.ToJson(), before);
    EXPECT_EQ(TWeaponProbeProfile::Create({}).MaximumReach(), 0);
    EXPECT_EQ(TWeaponProbeProfile::Create({}).TotalDiceSize(), 0);

    auto context = journal_->ExecutionContext();
    const auto initial = context.GetState();
    auto weapon = context.Project(first.Weapons(), [](TIdCollection<TWeapon>& values) {
        return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
    });
    auto reach = context.Project(weapon, [](TWeapon& value) { return value.Reach(); });
    context.AssignGuarded(reach, 7, weapon, "reach");

    EXPECT_EQ(first.MaximumReach(), 7);
    EXPECT_EQ(second.MaximumReach(), 3);
    EXPECT_EQ(first.Weapons()->Find(TId<TWeapon>{2})->Reach(), 1);
    EXPECT_EQ(CreateProbeLight().Reach().Get(), 1);
    context.Undo(initial);
    EXPECT_EQ(first.ToJson(), before);
}

TEST_F(TGeneratedSourcesTest, JournalDistinguishesPreviousCollectionIdsAndAllocationState)
{
    const auto snapshot = [](const TCreature& creature, const TTransformator& journal) {
        TAstContext context;
        context.RegisterIdentity(&creature, "creature");
        auto node = TAstNode::MakeObject("equipment_replacement");
        AddOwnedObject(node, "creature", creature, context);
        AddOwnedObject(node, "journal", journal, context);
        return node;
    };
    const auto weapons = [](std::size_t id, std::size_t next) {
        TIdCollection<TWeapon> result;
        result.RestoreEntry(TId<TWeapon>{id}, CreateProbeLight());
        result.RestoreNextId(TId<TWeapon>{next});
        return result;
    };
    const auto& catalog = battle_->AbilityRules().Catalog;
    for (const auto& [other_id, other_next] :
         std::vector<std::pair<std::size_t, std::size_t>>{{4, 7}, {2, 9}}) {
        SCOPED_TRACE("other id=" + std::to_string(other_id)
                     + " next=" + std::to_string(other_next));
        auto first = TCreature::Create({.Weapons = weapons(2, 7)});
        auto second = TCreature::Create({.Weapons = weapons(other_id, other_next)});
        TTransformator first_journal(io_);
        TTransformator second_journal(io_);
        auto first_context = first_journal.ExecutionContext();
        auto second_context = second_journal.ExecutionContext();
        const auto first_start = first_context.GetState();
        const auto second_start = second_context.GetState();
        const auto first_json = first.ToJson();
        const auto second_json = second.ToJson();
        const auto first_before = snapshot(first, first_journal);
        const auto second_before = snapshot(second, second_journal);
        const auto first_grants = CollectAbilities(first, catalog).Grants();
        const auto second_grants = CollectAbilities(second, catalog).Grants();
        ASSERT_EQ(first_grants.size(), 1u);
        ASSERT_EQ(second_grants.size(), 1u);

        TIdCollection<TWeapon> replacement;
        replacement.Add(CreateProbeHeavy());
        first_context.AssignGuarded(first.Weapons(), replacement,
                                    TGuarded<TCreature>(first), "weapons");
        second_context.AssignGuarded(second.Weapons(), replacement,
                                     TGuarded<TCreature>(second), "weapons");

        EXPECT_EQ(first.ToJson()["weapons"], second.ToJson()["weapons"]);
        TAstContext first_ast_context;
        TAstContext second_ast_context;
        first_ast_context.RegisterIdentity(&first, "creature");
        second_ast_context.RegisterIdentity(&second, "creature");
        EXPECT_EQ(first.GetAst(first_ast_context), second.GetAst(second_ast_context));
        EXPECT_NE(snapshot(first, first_journal), snapshot(second, second_journal));

        first_context.Undo(first_start);
        second_context.Undo(second_start);
        EXPECT_EQ(first.ToJson(), first_json);
        EXPECT_EQ(second.ToJson(), second_json);
        EXPECT_EQ(snapshot(first, first_journal), first_before);
        EXPECT_EQ(snapshot(second, second_journal), second_before);
        EXPECT_FALSE(first.Weapons()->Contains(TId<TWeapon>{0}));
        EXPECT_EQ(first.Weapons()->NextId().Value, 7u);
        EXPECT_EQ(second.Weapons()->NextId().Value, other_next);
        const auto first_restored = CollectAbilities(first, catalog).Grants();
        const auto second_restored = CollectAbilities(second, catalog).Grants();
        ASSERT_EQ(first_restored.size(), 1u);
        ASSERT_EQ(second_restored.size(), 1u);
        EXPECT_EQ(first_restored.front().Source, WeaponSource(2));
        EXPECT_EQ(second_restored.front().Source, WeaponSource(other_id));
        EXPECT_EQ(first_restored.front().Provenance, first_grants.front().Provenance);
        EXPECT_EQ(second_restored.front().Provenance, second_grants.front().Provenance);
        auto allocated = first.Weapons().Get();
        EXPECT_EQ(allocated.Add(CreateProbeHeavy()).Value, 7u);
    }
}

TEST_F(TGeneratedSourcesTest, SameNameWeaponsProduceSeparateOptionsAndOnlyChooseTheTarget)
{
    auto& actor = Add("Actor", CreateProbeFighter());
    auto& first = AddTarget("First");
    auto& second = AddTarget("Second");
    const auto before = Snapshot();
    const auto options = battle_->AvailableActions(actor);
    ASSERT_EQ(options.size(), 2u);
    EXPECT_EQ(Snapshot(), before);
    const auto light = Option(actor, WeaponSource(0));
    const auto heavy = Option(actor, WeaponSource(1));
    EXPECT_NE(light.Source, heavy.Source);
    EXPECT_EQ(light.DefinitionId, heavy.DefinitionId);
    EXPECT_FALSE(light.Provenance.empty());
    EXPECT_FALSE(heavy.Provenance.empty());

    io_.ExpectTarget("Second");
    rng_.ExpectCall(12, 7);
    EXPECT_EQ(battle_->Execute(heavy), EAbilityExecutionResult::Applied);
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 85);
    EXPECT_EQ(io_.Candidates(), (std::vector<std::string>{"Actor", "First", "Second"}));

    io_.ExpectTarget("First");
    rng_.ExpectCall(4, 3);
    EXPECT_EQ(battle_->Execute(light), EAbilityExecutionResult::Applied);
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Current(), 92);
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 85);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 1);
}

TEST_F(TGeneratedSourcesTest, RootFeatureCannotSatisfyTheDeclaredWeaponSource)
{
    TIdCollection<TFeatureSelection> features;
    features.Add(TFeatureSelection("generated_sources.ProbeTraining"));
    auto& actor = Add("Actor", TCreature::Create({
        .Hitpoints = THitPoints::Create({
            .Maximum = Statistic::Create({.Base = 100}),
            .Current = 100,
        }),
        .Features = features,
        .Actions = TResource(3),
    }));
    AddTarget("Target");
    const auto before = Snapshot();
    const auto grants = CollectAbilities(*actor.GetCreature(), battle_->AbilityRules().Catalog);
    ASSERT_EQ(grants.Grants().size(), 1u);
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    const auto& grant = grants.Grants().front();
    const TAbilityOption forged{
        .ActorId = actor.GetId(),
        .ActorIdentity = &actor,
        .DefinitionId = grant.DefinitionId,
        .Source = grant.Source,
        .Provenance = grant.Provenance,
        .Action = battle_->AbilityRules().Executions.Require(grant.DefinitionId).Action,
        .Trigger = {},
    };
    EXPECT_EQ(battle_->Execute(forged), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(ExecuteJournal(actor, forged), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedSourcesTest, NaturalWeaponUsesItsNearestProviderWithoutAnEquipmentFieldName)
{
    TIdCollection<TWeapon> natural;
    natural.Add(CreateProbeHeavy());
    auto& actor = Add("Actor", TCreature::Create({
        .Hitpoints = THitPoints::Create({
            .Maximum = Statistic::Create({.Base = 100}),
            .Current = 100,
        }),
        .NaturalWeapons = natural,
        .Actions = TResource(3),
    }));
    auto& target = AddTarget("Target");
    const auto option = Option(actor, TAbilitySourcePath{}.Field("natural_weapons")
        .Entry(0).Field("features").Entry(0));
    io_.ExpectTarget("Target");
    rng_.ExpectCall(12, 6);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 86);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
}

TEST_F(TGeneratedSourcesTest, NamedCreatureAndSnapshotFactoriesOwnIndependentWeaponValues)
{
    auto& actor = Add("Actor", CreateProbeFighter());
    auto& copy = Add("Copy", CreateProbeFighter());
    TGameObjectFactory factory;
    const auto snapshot = actor.GetCreature()->ToJson();
    auto restored = TCreature::Restore(snapshot, factory);
    EXPECT_NE(actor.GetCreature()->Id(), copy.GetCreature()->Id());
    EXPECT_EQ(actor.GetCreature()->Id(), restored.Id());
    EXPECT_NE(actor.GetCreature(), &restored);
    const auto saved = Option(actor, WeaponSource(0));

    auto context = journal_->ExecutionContext();
    const auto initial = context.GetState();
    auto weapons = actor.GetCreature()->Weapons().Get();
    weapons.Remove(TId<TWeapon>{0});
    context.AssignGuarded(actor.GetCreature()->Weapons(), std::move(weapons),
                          TGuarded<TCreature>(*actor.GetCreature()), "weapons");
    EXPECT_EQ(battle_->AvailableActions(actor).size(), 1u);
    EXPECT_EQ(battle_->AvailableActions(copy).size(), 2u);
    EXPECT_EQ(restored.Weapons()->Size(), 2u);
    EXPECT_EQ(restored.ToJson(), snapshot);
    EXPECT_EQ(battle_->Execute(saved), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    EXPECT_EQ(copy.GetCreature()->Actions()->Count(), 3);

    context.Undo(initial);
    const auto after = Option(actor, WeaponSource(0));
    EXPECT_EQ(after.Source, saved.Source);
    EXPECT_EQ(after.Provenance, saved.Provenance);
}

TEST_F(TGeneratedSourcesTest, SuspendedSourceRemovalRejectsBeforeRollAndUndoAllowsReplay)
{
    auto& actor = Add("Actor", CreateProbeFighter());
    auto& target = AddTarget("Target");
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    io_.ExpectTarget("Target", true);
    rng_.ExpectCall(4, 3);
    const auto point = Suspend([&]() { ExecuteJournal(actor, Option(actor, WeaponSource(0))); });
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(rng_.RemainingCalls(), 1u);

    TTransformator source_editor(io_);
    auto context = source_editor.ExecutionContext();
    const auto edit_start = context.GetState();
    auto weapons = actor.GetCreature()->Weapons().Get();
    weapons.Remove(TId<TWeapon>{0});
    context.AssignGuarded(actor.GetCreature()->Weapons(), std::move(weapons),
                          TGuarded<TCreature>(*actor.GetCreature()), "weapons");
    EXPECT_EQ(battle_->AvailableActions(actor).size(), 1u);
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(rng_.RemainingCalls(), 1u);

    context.Undo(edit_start);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 92);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    point.Revert(*journal_);
    rng_.ExpectCall(4, 2);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 93);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedSourcesTest, SuspendedExecutionKeepsSourceValuesAndNextExecutionTakesNewSnapshot)
{
    auto& actor = Add("Actor", CreateProbeFighter());
    auto& target = AddTarget("Target");
    const auto option = Option(actor, WeaponSource(0));
    io_.ExpectTarget("Target", true);
    rng_.ExpectCall(4, 3);
    const auto point = Suspend([&]() { battle_->Execute(option); });

    auto context = journal_->ExecutionContext();
    const auto initial = context.GetState();
    auto weapon = context.Project(actor.GetCreature()->Weapons(), [](TIdCollection<TWeapon>& values) {
        return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
    });
    const auto original_weapon = weapon.Get().ToJson();
    auto die = context.Project(weapon, [](TWeapon& value) { return value.BaseDiceSize(); });
    auto traits = context.Project(weapon, [](TWeapon& value) { return value.Traits(); });
    const auto heavy = CreateProbeHeavy();
    context.AssignGuarded(die, 12, weapon, "base_dice_size");
    context.AssignGuarded(traits, heavy.Traits(), weapon, "traits");
    EXPECT_EQ(battle_->AvailableActions(actor).size(), 2u);

    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 92);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    io_.ExpectTarget("Target");
    rng_.ExpectCall(12, 6);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 79);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 1);
    context.Undo(initial);
    EXPECT_EQ(actor.GetCreature()->Weapons()->Find(TId<TWeapon>{0})->ToJson(), original_weapon);
}

}

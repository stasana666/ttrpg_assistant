#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/inventory/generated_world.h>

#include "../test_lib/battle_fixture.h"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

TIdCollection<TFeatureSelection> WorldFeatures(const std::string& name)
{
    TIdCollection<TFeatureSelection> features;
    if (!name.empty()) {
        features.Add(TFeatureSelection("generated_world." + name));
    }
    return features;
}

THitPoints WorldHitpoints(int current)
{
    return THitPoints::Create({
        .Maximum = Statistic::Create({.Base = 100}),
        .Current = current,
    });
}

class TGeneratedWorldTest : public TBattleFixture {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedWorldRules(*rules);
        StartBattle(std::move(rules), 8, "generated_world.");
    }

    TPlayer& Actor(int reach = 1, const std::string& feature = {}, bool class_feature = false)
    {
        const auto blade = CreateWorldBlade();
        TIdCollection<TWeapon> weapons;
        weapons.Add(TWeapon::Create({
            .Name = blade.Name(),
            .BaseDiceSize = blade.BaseDiceSize(),
            .DamageType = blade.DamageType(),
            .Category = blade.Category(),
            .Reach = reach,
            .Features = blade.Features(),
        }));
        return Join(TCreature::Create({
            .Level = 1,
            .Class = TClass::Create({
                .Features = WorldFeatures(class_feature ? feature : ""),
            }),
            .Hitpoints = WorldHitpoints(100),
            .Weapons = weapons,
            .Features = WorldFeatures(class_feature ? "" : feature),
            .Actions = TResource(3),
        }), 0, {0, 0}, "Actor");
    }

    TPlayer& Participant(std::string name, TPosition position, int team = 1, int current = 100)
    {
        return Join(TCreature::Create({
            .Level = 1,
            .Hitpoints = WorldHitpoints(current),
        }), team, position, std::move(name));
    }

    TAbilityOption Option(TPlayer& actor, const std::string& name = "WorldProbe")
    {
        return TBattleFixture::Option(name, actor.GetId());
    }

    TAbilityOption UncheckedOption(TPlayer& actor) const
    {
        const auto grants = CollectAbilities(*actor.GetCreature(), battle_->AbilityRules().Catalog);
        const auto& grant = grants.Grants().at(0);
        return {
            .ActorId = actor.GetId(),
            .ActorIdentity = &actor,
            .DefinitionId = grant.DefinitionId,
            .Source = grant.Source,
            .Provenance = grant.Provenance,
            .Action = battle_->AbilityRules().Executions.Require(grant.DefinitionId).Action,
        };
    }
};

TEST_F(TGeneratedWorldTest, NoEligibleTargetsHideTheActionAndRejectExecutionBeforePayment)
{
    auto& actor = Actor();
    Participant("Ally", {1, 0}, 0);
    Participant("Defeated", {0, 1}, 1, 0);
    Participant("Distant", {4, 4});
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    const auto forged = UncheckedOption(actor);
    EXPECT_EQ(battle_->Execute(forged), EAbilityExecutionResult::Unavailable);
    Act(forged, {.Result = EAbilityExecutionResult::Unavailable});
}

TEST_F(TGeneratedWorldTest, TargetFilterUsesChebyshevDistanceTeamsAndLivingParticipants)
{
    auto& actor = Actor();
    Participant("Diagonal", {1, 1});
    Participant("Straight", {1, 0});
    Participant("Ally", {0, 1}, 0);
    Participant("Distant", {3, 3});
    Act(Option(actor), {.Choose = {"Diagonal"}, .Rolls = {{6, 4}},
        .Changes = {{"Diagonal.hitpoints.current", 96}, {"Actor.actions.count", 2}}});
    EXPECT_EQ(io_.Candidates(), (std::vector<std::string>{"Diagonal", "Straight"}));
}

TEST_F(TGeneratedWorldTest, StaleOptionsRecheckMovedOrDefeatedTargetsBeforePayment)
{
    auto& actor = Actor();
    auto& target = Participant("Target", {1, 0});
    const auto option = Option(actor);
    const auto start = journal_->CurrentState();
    journal_->MovePlayer(&target, {5, 0});
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    Act(option, {.Result = EAbilityExecutionResult::Unavailable});
    journal_->Undo(start);
    journal_->DealDamage(&target, 100);
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    Act(option, {.Result = EAbilityExecutionResult::Unavailable});
    journal_->Undo(start);
    Act(option, {.Choose = {"Target"}, .Rolls = {{6, 2}},
        .Changes = {{"Target.hitpoints.current", 98}, {"Actor.actions.count", 2}}});
}

TEST_F(TGeneratedWorldTest, WeaponReachIsSnapshottedWhileTargetCoordinatesRemainLive)
{
    auto& actor = Actor();
    auto& target = Participant("Target", {1, 0});
    const auto option = Option(actor);
    io_.ExpectName(0, {}, "Target", true);
    rng_.ExpectCall(6, 4);
    const auto first = Pause([&]() { battle_->Execute(option); });
    auto context = journal_->ExecutionContext();
    const auto initial = context.GetState();
    auto weapon = context.Project(actor.GetCreature()->Weapons(), [](TIdCollection<TWeapon>& values) {
        return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
    });
    auto reach = context.Project(weapon, [](TWeapon& value) { return value.Reach(); });
    context.AssignGuarded(reach, 3, weapon, "reach");
    const auto extended = context.GetState();
    journal_->MovePlayer(&target, {2, 0});
    EXPECT_EQ(battle_->AvailableActions(actor).size(), 1u);
    EXPECT_THROW(first.Resume(), TAbilityUnavailable);
    EXPECT_EQ(rng_.RemainingCalls(), 1u);

    context.Undo(extended);
    first.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 96);
    journal_->MovePlayer(&target, {2, 0});
    io_.ExpectName(0, {}, "Target", true);
    rng_.ExpectCall(6, 2);
    const auto second = Pause([&]() { battle_->Execute(option); });
    context.AssignGuarded(reach, 1, weapon, "reach");
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    second.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 94);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 1);
    context.Undo(initial);
    EXPECT_EQ(target.GetPosition(), (TPosition{1, 0}));
    EXPECT_EQ(actor.GetCreature()->Weapons()->Find(TId<TWeapon>{0})->Reach(), 1);
}

TEST_F(TGeneratedWorldTest, TwoWolvesDoNotContributeUntilAnotherLivingAllyJoins)
{
    auto& actor = Actor(1, "TwoOtherAllies");
    Participant("Target", {1, 0});
    Participant("Wolf B", {1, 1}, 0);
    Participant("Other enemy", {0, 1});
    Participant("Defeated ally", {2, 0}, 0, 0);
    const auto option = Option(actor);
    Act(option, {.Choose = {"Target"}, .Rolls = {{6, 3}},
        .Changes = {{"Target.hitpoints.current", 97}, {"Actor.actions.count", 2}}});
    Participant("Wolf C", {2, 1}, 0);
    Act(option, {.Choose = {"Target"}, .Rolls = {{6, 3}, {4, 2}},
        .Changes = {{"Target.hitpoints.current", 92}, {"Actor.actions.count", 1}}});
}

TEST_F(TGeneratedWorldTest, EventDispatchReadsCurrentFeatureProvidersAndAllyPositions)
{
    auto& actor = Actor(1, "TwoOtherAllies", true);
    auto& target = Participant("Target", {1, 0});
    Participant("First ally", {1, 1}, 0);
    auto& other = Participant("Second ally", {2, 0}, 0);
    const auto option = Option(actor);
    io_.ExpectName(0, {}, "Target", true);
    rng_.ExpectCall(6, 3);
    const auto point = Pause([&]() { battle_->Execute(option); });
    auto context = journal_->ExecutionContext();
    const auto initial = context.GetState();
    context.AssignGuarded(actor.GetCreature()->Class(), TClass::Create({}),
                          TGuarded<TCreature>(*actor.GetCreature()), "class");
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 97);
    context.Undo(initial);

    journal_->MovePlayer(&other, {5, 5});
    io_.ExpectName(0, {}, "Target");
    rng_.ExpectCall(6, 3);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 94);
    context.Undo(initial);
    io_.ExpectName(0, {}, "Target");
    rng_.ExpectCall(6, 3);
    rng_.ExpectCall(4, 2);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 89);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 0);
}

TEST_F(TGeneratedWorldTest, PrefixRequirementsGateFreshExecutionsWithoutCancellingPaidActions)
{
    auto& actor = Actor();
    auto& target = Participant("Target", {1, 0});
    const auto option = Option(actor);
    auto context = journal_->ExecutionContext();
    const auto initial = context.GetState();
    context.AssignGuarded(actor.GetCreature()->Level(), 0,
                          TGuarded<TCreature>(*actor.GetCreature()), "level");
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    Act(option, {.Result = EAbilityExecutionResult::Unavailable});
    context.Undo(initial);

    io_.ExpectName(0, {}, "Target", true);
    rng_.ExpectCall(6, 4);
    const auto point = Pause([&]() { battle_->Execute(option); });
    context.AssignGuarded(actor.GetCreature()->Level(), 0,
                          TGuarded<TCreature>(*actor.GetCreature()), "level");
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(rng_.RemainingCalls(), 1u);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 96);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    context.Undo(initial);
    EXPECT_EQ(battle_->AvailableActions(actor).size(), 1u);
}

TEST_F(TGeneratedWorldTest, ResourceRequirementUsesPrepaymentBudgetAndAllowsResumeAfterSpending)
{
    auto& actor = Actor(1, "BudgetTraining");
    auto& target = Participant("Target", {2, 0});
    const auto option = Option(actor, "BudgetProbe");
    io_.ExpectName(0, {}, "Target", true);
    const auto point = Pause([&]() { battle_->Execute(option); });
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 100);
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Unavailable);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 99);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
}

TEST_F(TGeneratedWorldTest, PositionBuiltinsUseCurrentCoordinatesInRequirementsAndBody)
{
    auto& actor = Actor(1, "CoordinateTraining");
    auto& target = Participant("Target", {2, 3});
    const auto option = Option(actor, "CoordinateProbe");
    const auto initial = journal_->CurrentState();
    journal_->MovePlayer(&actor, {0, 1});
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    Act(option, {.Result = EAbilityExecutionResult::Unavailable});
    journal_->Undo(initial);

    io_.ExpectName(0, {}, "Target", true);
    const auto point = Pause([&]() { battle_->Execute(option); });
    journal_->MovePlayer(&target, {4, 2});
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 58);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
}

}

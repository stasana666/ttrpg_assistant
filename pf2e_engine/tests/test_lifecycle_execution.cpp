#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/lifecycle_execution.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/rules.h>
#include <pf2e_engine/inventory/generated_lifecycle.h>

#include "test_lib/mock_dice_roller.h"
#include "test_lib/scripted_io.h"
#include "test_lib/mock_interaction_system.h"
#include "test_lib/journal_setup.h"

#include <gtest/gtest.h>

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

std::shared_ptr<const TLifecycleExecution> Hook(TLifecycleExecution::FRun body)
{
    return std::make_shared<const TLifecycleExecution>(TLifecycleExecution{
        .OwnerType = typeid(TCreature), .Run = std::move(body),
    });
}

TCreature::TInitialValues Values(int current = 20, bool tough = false)
{
    TIdCollection<TFeatureSelection> features;
    features.Add(TFeatureSelection("generated_lifecycle.LifecycleTraining"));
    TIdCollection<TFeatureSelection> ancestry_features;
    if (tough) {
        ancestry_features.Add(TFeatureSelection("generated_lifecycle.InheritedToughness"));
    }
    TVariantMap<EConditionKind, TCondition> conditions;
    if (current == 0) {
        conditions.Set(TConditionDying{.Value = 1});
        conditions.Set(TConditionUnconscious{});
        conditions.Set(TConditionProne{});
    }
    return {
        .Race = TRace::Create({.Features = std::move(ancestry_features)}),
        .Hitpoints = THitPoints::Create({
            .Maximum = Statistic::Create({.Base = 20}), .Current = current,
        }),
        .Features = std::move(features), .Conditions = std::move(conditions),
        .ZeroHpRule = EZeroHpRule::Dying, .AttacksMade = 2,
        .Actions = TResource(0), .Reactions = TResource(0), .Hands = TResource(0),
    };
}

TSavepointStackUnwind Suspend(const std::function<void()>& work)
{
    try {
        work();
    } catch (const TSavepointStackUnwind& point) {
        return point;
    }
    throw std::logic_error("lifecycle did not suspend");
}

class TLifecycleExecutionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedLifecycleRules(*rules);
        battle_ = std::make_unique<TBattle>(
            TBattleMap(3, 1), &rng_, io_, rules);
    }

    void TearDown() override
    {
        io_.Verify();
        rng_.Verify();
    }

    TPlayer& Add(int current = 0, bool tough = false)
    {
        return AddValues(Values(current, tough));
    }

    TPlayer& AddValues(TCreature::TInitialValues values)
    {
        const int id = static_cast<int>(creatures_.size());
        creatures_.push_back(TCreature::Create(std::move(values)));
        rng_.ExpectCall(20, 10);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id}, TPlayerId{id},
                                  "Participant", ""), {id, 0});
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    std::shared_ptr<TActionContext> Services()
    {
        return std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = &journal_,
            .io_system = &io_,
        });
    }

    TAstNode Snapshot() const
    {
        TAstContext context;
        auto node = TAstNode::MakeObject("system_lifecycle");
        AddOwnedObject(node, "battle", *battle_, context);
        AddOwnedObject(node, "journal", journal_, context);
        return node;
    }

    TScriptedIo io_;
    TMockRng rng_;
    TTransformator journal_{io_};
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
};

TEST(LifecycleRegistryTest, ConflictsAndInvalidRegistrationsPreserveExistingHook)
{
    TLifecycleExecutions registry;
    const auto hook = Hook([](auto, auto) {});
    EXPECT_EQ(registry.Find(ELifecyclePhase::TurnStart), nullptr);
    registry.Register(ELifecyclePhase::TurnStart, hook);
    EXPECT_NO_THROW(registry.Register(ELifecyclePhase::TurnStart, hook));
    EXPECT_THROW(registry.Register(ELifecyclePhase::TurnStart, Hook([](auto, auto) {})),
                 std::invalid_argument);
    EXPECT_THROW(registry.Register(ELifecyclePhase::TurnEnd, nullptr), std::invalid_argument);
    EXPECT_THROW(registry.Register(ELifecyclePhase::TurnEnd,
                                  std::make_shared<const TLifecycleExecution>()),
                 std::invalid_argument);
    EXPECT_THROW(registry.Register(static_cast<ELifecyclePhase>(100), hook), std::invalid_argument);
    EXPECT_THROW(registry.Find(static_cast<ELifecyclePhase>(100)), std::invalid_argument);
    EXPECT_EQ(registry.Find(ELifecyclePhase::TurnStart), hook);
    EXPECT_EQ(registry.Find(ELifecyclePhase::TurnEnd), nullptr);
}

TEST_F(TLifecycleExecutionTest, TwoSuspensionsKeepInactiveActorAndReplayWithoutReroll)
{
    auto& actor = Add();
    auto& creature = *actor.GetCreature();
    auto services = Services();
    const auto initial = journal_.CurrentState();
    const auto before = Snapshot();
    int completions = 0;
    rng_.ExpectCall(20, 7);
    io_.ExpectValue(0, 2, true);
    const auto first = Suspend([&] {
        InvokeLifecycle(services, actor, Hook(RunGeneratedLifecyclePausedTurnStart),
                        [&] { ++completions; });
    });
    EXPECT_EQ(creature.Hands()->Count(), 1);
    EXPECT_EQ(creature.Actions()->Count(), 0);
    EXPECT_FALSE(creature.CanAct());
    services->battle = nullptr;
    services->dice_roller = nullptr;
    services->transformator = nullptr;
    services.reset();
    io_.ExpectValue(0, 3, true);
    const auto second = Suspend([&] { first.Resume(); });
    EXPECT_EQ(creature.Hands()->Count(), 10);
    EXPECT_EQ(completions, 0);
    second.Resume();
    EXPECT_EQ(creature.Hands()->Count(), 13);
    EXPECT_EQ(creature.Actions()->Count(), 3);
    EXPECT_EQ(creature.Reactions()->Count(), 1);
    EXPECT_EQ(creature.AttacksMade().Get(), 0);
    EXPECT_FALSE(creature.CanAct());
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    EXPECT_EQ(completions, 1);
    const auto completed = Snapshot();
    second.Revert(journal_);
    second.Resume();
    EXPECT_EQ(Snapshot(), completed);
    EXPECT_EQ(completions, 2);
    first.Revert(journal_);
    io_.ExpectValue(0, 3, false);
    first.Resume();
    EXPECT_EQ(Snapshot(), completed);
    EXPECT_EQ(completions, 3);
    journal_.Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TLifecycleExecutionTest, DrainCompletesEachSuspensionAndDeadActorCleanup)
{
    auto& actor = Add();
    auto& creature = *actor.GetCreature();
    auto execution = journal_.ExecutionContext();
    execution.Invoke(TGuarded<TCreature>(creature), &TCreature::Die);
    const auto start = journal_.CurrentState();
    const auto before = Snapshot();
    rng_.ExpectCall(20, 7);
    io_.ExpectValue(0, 4, true);
    io_.ExpectValue(0, 5, true);
    RunLifecycleAndDrain(Services(), actor, Hook(RunGeneratedLifecyclePausedTurnStart));
    EXPECT_EQ(creature.Hands()->Count(), 17);
    EXPECT_EQ(creature.Actions()->Count(), 0);
    EXPECT_EQ(creature.Reactions()->Count(), 0);
    EXPECT_TRUE(creature.IsDead());
    journal_.Undo(start);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TLifecycleExecutionTest, InvalidWorldAndOwnerRejectBeforeBodyRuns)
{
    auto& actor = Add();
    int calls = 0;
    const auto hook = Hook([&](auto, auto) { ++calls; });
    auto services = Services();
    auto foreign_creature = TCreature::Create(Values());
    TPlayer foreign(&foreign_creature, TPlayerTeam{1}, TPlayerId{1}, "Foreign", "");
    const auto before = Snapshot();
    EXPECT_THROW(InvokeLifecycle(services, foreign, hook, [] {}), TAbilityUnavailable);
    const auto wrong_owner = std::make_shared<const TLifecycleExecution>(TLifecycleExecution{
        .OwnerType = typeid(TWeapon), .Run = hook->Run,
    });
    EXPECT_THROW(InvokeLifecycle(services, actor, wrong_owner, [] {}), std::invalid_argument);
    EXPECT_THROW(InvokeLifecycle(services, actor, hook, {}), std::invalid_argument);
    services->battle = nullptr;
    EXPECT_THROW(InvokeLifecycle(services, actor, hook, [] {}), std::invalid_argument);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TLifecycleExecutionTest, ImmediateAndFailedBodiesCannotInvokeLateContinuation)
{
    auto& actor = Add();
    std::function<void()> late;
    int completions = 0;
    InvokeLifecycle(Services(), actor, Hook([&](auto, auto done) { late = std::move(done); }),
                    [&] { ++completions; });
    EXPECT_EQ(completions, 0);
    ASSERT_TRUE(late);
    EXPECT_THROW(late(), std::logic_error);
    EXPECT_THROW(InvokeLifecycle(Services(), actor, Hook([&](auto, auto done) {
        late = std::move(done);
        throw std::runtime_error("body failed");
    }), [&] { ++completions; }), std::runtime_error);
    EXPECT_THROW(late(), std::logic_error);
    EXPECT_EQ(completions, 0);
}

TEST_F(TLifecycleExecutionTest, OrdinaryExecutionCannotReuseSystemPrivilege)
{
    auto& actor = Add(20);
    const auto options = battle_->AvailableActions(actor);
    ASSERT_EQ(options.size(), 1);
    std::shared_ptr<TActionContext> captured;
    const auto hook = Hook([&](auto context, auto) { captured = std::move(context); });
    InvokeLifecycle(Services(), actor, hook, [] {});
    ASSERT_TRUE(captured);
    EXPECT_EQ(captured->execution_mode, EActionExecutionMode::System);
    auto execution = journal_.ExecutionContext();
    execution.Invoke(TGuarded<TCreature>(*actor.GetCreature()), &TCreature::BecomeUnconscious);
    const auto before = Snapshot();
    const auto& rules = battle_->AbilityRules();
    EXPECT_EQ(ExecuteAbility(options.front(), captured, actor,
        CollectAbilities(*actor.GetCreature(), rules.Catalog), rules.Executions),
        EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(captured->execution_mode, EActionExecutionMode::Ordinary);
    EXPECT_FALSE(captured->owned_lifecycle);
    InvokeLifecycle(Services(), actor, hook, [] {});
    EXPECT_THROW(options.front().Action->Apply(captured, actor), TAbilityUnavailable);
    EXPECT_EQ(captured->execution_mode, EActionExecutionMode::Ordinary);
    EXPECT_FALSE(captured->owned_lifecycle);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TLifecycleExecutionTest, RecoveryUsesTransitiveFeatureAndCurrentGrants)
{
    auto& actor = Add(0, true);
    auto& creature = *actor.GetCreature();
    const auto start = journal_.CurrentState();
    const auto before = Snapshot();
    const auto hook = Hook(RunAbilitiesStartCreatureTurn);
    rng_.ExpectCall(20, 10);
    RunLifecycleAndDrain(Services(), actor, hook);
    EXPECT_EQ(creature.DyingValue(), 0);
    EXPECT_EQ(creature.WoundedValue(), 1);
    EXPECT_EQ(creature.Hitpoints()->Current(), 0);
    EXPECT_EQ(creature.Actions()->Count(), 3);
    EXPECT_EQ(creature.Reactions()->Count(), 1);
    EXPECT_FALSE(creature.CanAct());
    journal_.Undo(start);
    EXPECT_EQ(Snapshot(), before);
    auto execution = journal_.ExecutionContext();
    execution.AssignGuarded(creature.Race(), TRace::Create({}),
                            TGuarded<TCreature>(creature), "race");
    rng_.ExpectCall(20, 10);
    RunLifecycleAndDrain(Services(), actor, hook);
    EXPECT_EQ(creature.DyingValue(), 2);
    EXPECT_EQ(creature.WoundedValue(), 0);
    journal_.Undo(start);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TLifecycleExecutionTest, SuspendedLegacyBodyStopsWhenActorBecomesUnableToAct)
{
    for (const bool dies : {false, true}) {
        auto& actor = Add(20);
        auto& creature = *actor.GetCreature();
        TAction action([&creature](std::shared_ptr<TActionContext> context) {
            AddResource(*context->transformator, creature.Hands(), 1);
            throw TSavepointStackUnwind(context->transformator->CurrentState(),
                [context, &creature] {
                    AddResource(*context->transformator, creature.Hands(), 10);
                });
        }, TAction::TBoundResources{}, "Legacy probe");
        const auto point = Suspend([&] { action.Apply(Services(), actor); });
        auto execution = journal_.ExecutionContext();
        if (dies) {
            execution.Invoke(TGuarded<TCreature>(creature), &TCreature::Die);
        } else {
            execution.Invoke(TGuarded<TCreature>(creature), &TCreature::BecomeUnconscious);
        }
        const auto before = Snapshot();
        EXPECT_THROW(point.Resume(), TAbilityUnavailable);
        EXPECT_EQ(creature.Hands()->Count(), 1);
        EXPECT_EQ(Snapshot(), before);
    }
}

TEST_F(TLifecycleExecutionTest, UnconsciousPenaltiesMergeWithoutStackingWithFrightened)
{
    auto values = Values(20);
    const auto baseline = TStatisticBaseline::Create({.IsFixed = true, .Value = 20});
    values.PerceptionBaseline = baseline;
    values.ArmorClassBaseline = baseline;
    values.SaveBaselines = std::map<ESavethrow, TStatisticBaseline>{
        {ESavethrow::Fortitude, baseline}, {ESavethrow::Reflex, baseline},
        {ESavethrow::Will, baseline},
    };
    auto& actor = AddValues(std::move(values));
    auto& creature = *actor.GetCreature();
    auto execution = journal_.ExecutionContext();
    const auto start = journal_.CurrentState();
    const auto before = Snapshot();
    execution.Invoke(TGuarded<TCreature>(creature), &TCreature::BecomeUnconscious);
    EXPECT_EQ(creature.ArmorClass(), 14);
    EXPECT_EQ(creature.PerceptionBonus(), 16);
    EXPECT_EQ(creature.SaveBonus(ESavethrow::Reflex), 16);
    EXPECT_EQ(creature.SaveBonus(ESavethrow::Fortitude), 20);
    EXPECT_EQ(creature.SaveBonus(ESavethrow::Will), 20);
    SetCondition(journal_, creature, TConditionFrightened{.Value = 2});
    EXPECT_EQ(creature.ArmorClass(), 14);
    EXPECT_EQ(creature.PerceptionBonus(), 16);
    EXPECT_EQ(creature.SaveBonus(ESavethrow::Reflex), 16);
    EXPECT_EQ(creature.SaveBonus(ESavethrow::Fortitude), 18);
    const auto extra = TCheckModifiers::Create({.StatusPenalty = 6, .CircumstancePenalty = 1});
    EXPECT_EQ(creature.PerceptionBonusWithModifiers(extra), 13);
    EXPECT_EQ(creature.SaveBonusWithModifiers(ESavethrow::Reflex, extra), 13);
    SetCondition(journal_, creature, TConditionFrightened{.Value = 5});
    EXPECT_EQ(creature.ArmorClass(), 13);
    execution.Invoke(TGuarded<TCreature>(creature), &TCreature::ReceiveHealing, 1);
    EXPECT_EQ(creature.ArmorClass(), 13);
    EXPECT_EQ(creature.PerceptionBonus(), 15);
    EXPECT_TRUE(creature.CanAct());
    journal_.Undo(start);
    EXPECT_EQ(Snapshot(), before);
}

TEST(LifecycleBattleIntegrationTest, FactoryInstallsRecoveryAndBattleSkipsUnconsciousPrompts)
{
    auto rules = std::make_shared<TAbilityRules>(*CreateAbilityRules());
    RegisterGeneratedLifecycleRules(*rules);
    ASSERT_TRUE(rules->Lifecycle.Find(ELifecyclePhase::TurnStart));
    TMockInteractionSystem io;
    TMockRng rng;
    TBattle battle(TBattleMap(3, 1), &rng, io, rules);
    std::deque<TCreature> creatures;
    for (int id = 0; id < 3; ++id) {
        creatures.push_back(TCreature::Create(Values(id == 0 ? 0 : 20, id == 0)));
        rng.ExpectCall(20, id == 0 ? 20 : 10 - id);
        battle.AddPlayer(TPlayer(&creatures.back(), TPlayerTeam{id == 2 ? 1 : 0},
                                TPlayerId{id}, "Participant", ""), {id, 0});
    }
    rng.ExpectCall(20, 10);
    io.AddCheckCallback([&] {
        EXPECT_EQ(creatures[0].DyingValue(), 0);
        EXPECT_EQ(creatures[0].WoundedValue(), 1);
        EXPECT_EQ(creatures[0].Actions()->Count(), 0);
        EXPECT_EQ(creatures[0].Reactions()->Count(), 1);
        EXPECT_EQ(creatures[0].AttacksMade().Get(), 0);
        EXPECT_FALSE(creatures[0].CanAct());
        EXPECT_EQ(creatures[1].Actions()->Count(), 3);
    });
    io.ExpectChoice(1, "next action", "End of turn");
    EXPECT_THROW(battle.StartBattle(), TTooManyCallsError);
    EXPECT_EQ(creatures[2].Actions()->Count(), 3);
    io.Verify();
    rng.Verify();
}

}

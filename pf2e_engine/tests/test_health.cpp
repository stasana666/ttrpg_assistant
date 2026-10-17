#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

#include "test_lib/mock_interaction_system.h"
#include "test_lib/state_diff.h"
#include "test_lib/journal_setup.h"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

TCreature::TInitialValues HealthValues(EZeroHpRule policy, int current,
                                     int maximum = 20, int temporary = 0,
                                     int dying = 0, int wounded = 0,
                                     bool unconscious = false)
{
    TVariantMap<EConditionKind, TCondition> conditions;
    if (dying > 0) {
        conditions.Set(TConditionDying{.Value = dying});
        unconscious = true;
    }
    if (wounded > 0) {
        conditions.Set(TConditionWounded{.Value = wounded});
    }
    if (unconscious) {
        conditions.Set(TConditionUnconscious{});
        conditions.Set(TConditionProne{});
    }
    return {
        .Hitpoints = THitPoints::Create({
            .Maximum = Statistic::Create({.Base = maximum}),
            .Current = current, .Temporary = temporary,
        }),
        .Conditions = std::move(conditions), .ZeroHpRule = policy,
        .Actions = TResource(2), .Reactions = TResource(1),
    };
}

class HealthRulesTest : public ::testing::Test {
protected:
    THealthDamageResult Damage(TCreature& creature, int amount,
                               bool critical = false, bool nonlethal = false)
    {
        auto execution = journal_.ExecutionContext();
        return execution.Invoke(TGuarded<TCreature>(creature), &TCreature::ApplyResolvedDamage,
                                 amount, TDamageContext::Create({
                                     .Critical = critical, .Nonlethal = nonlethal,
                                 }));
    }

    int Heal(TCreature& creature, int amount)
    {
        auto execution = journal_.ExecutionContext();
        return execution.Invoke(TGuarded<TCreature>(creature), &TCreature::ReceiveHealing, amount);
    }

    ECheckOutcome Recover(TCreature& creature, int natural, int adjustment = 0)
    {
        auto execution = journal_.ExecutionContext();
        return execution.Invoke(TGuarded<TCreature>(creature), &TCreature::ApplyRecovery,
                                 natural, adjustment);
    }

    void Step(const TCreature& creature, const std::function<void()>& operation,
              const std::map<std::string, TField>& changes)
    {
        ExpectStep(creature, journal_, operation, changes);
    }

    TMockInteractionSystem io_;
    TTransformator journal_{io_};
};

TEST_F(HealthRulesTest, ZeroHitpointPoliciesSeparateLifeEligibilityAndDefeat)
{
    const auto legacy = TCreature::Create(HealthValues(EZeroHpRule::Defeat, 0));
    const auto dying = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 1));
    const auto dead = TCreature::Create(HealthValues(EZeroHpRule::Death, 0));
    const auto knocked_out = TCreature::Create(HealthValues(EZeroHpRule::Death, 0, 20, 0, 0, 0, true));
    EXPECT_FALSE(legacy.IsAlive());
    EXPECT_FALSE(legacy.IsDead());
    EXPECT_TRUE(dying.IsAlive());
    EXPECT_TRUE(dying.NeedsRecoveryCheck());
    EXPECT_TRUE(dead.IsDead());
    EXPECT_FALSE(dead.IsAlive());
    EXPECT_FALSE(knocked_out.IsDead());
    EXPECT_TRUE(knocked_out.IsAlive());
    EXPECT_FALSE(knocked_out.NeedsRecoveryCheck());
    for (const auto* creature : {&legacy, &dying, &dead, &knocked_out}) {
        EXPECT_FALSE(creature->CanAct());
        EXPECT_TRUE(creature->IsDefeated());
    }
}

TEST_F(HealthRulesTest, NormalAndCriticalKnockoutAddWoundedOnce)
{
    for (const bool critical : {false, true}) {
        auto creature = TCreature::Create(HealthValues(EZeroHpRule::Dying, 8, 20, 0, 0, 1));
        std::optional<THealthDamageResult> result;
        Step(creature, [&] { result.emplace(Damage(creature, 8, critical)); }, {
            {"hitpoints.current", 0}, {"conditions.Dying", true}, {"conditions.Dying.value", critical ? 3 : 2},
            {"conditions.Unconscious", true}, {"conditions.Prone", true}});
        EXPECT_TRUE(result->NewlyKnockedOut());
        EXPECT_FALSE(result->NewlyDead());
        EXPECT_FALSE(creature.CanAct());
    }
}
TEST_F(HealthRulesTest, DamageWhileDyingIncludesTemporaryAbsorptionAndDoesNotReaddWounded)
{
    auto creature = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 5, 1, 1));
    std::optional<THealthDamageResult> critical;
    Step(creature, [&] { critical.emplace(Damage(creature, 1, true)); },
         {{"hitpoints.temporary", 4}, {"conditions.Dying.value", 3}});
    EXPECT_EQ(critical->Hitpoints()->AbsorbedTemporary(), 1);
    EXPECT_FALSE(critical->NewlyKnockedOut());
    std::optional<THealthDamageResult> nonlethal;
    Step(creature, [&] { nonlethal.emplace(Damage(creature, 1, false, true)); },
         {{"hitpoints.temporary", 3}, {"conditions.Dying.value", 4}, {"dead", true}, {"actions.count", 0},
          {"reactions.count", 0}});
    EXPECT_TRUE(nonlethal->NewlyDead());
}
TEST_F(HealthRulesTest, PreventedDamageCannotIncreaseDying)
{
    auto values = HealthValues(EZeroHpRule::Dying, 0, 20, 3, 1);
    values.Immunities = std::set<EDamageType>{EDamageType::Fire};
    values.Resistances = std::map<EDamageType, int>{{EDamageType::Slashing, 10}};
    auto creature = TCreature::Create(values);
    Step(creature, [&] {
        auto execution = journal_.ExecutionContext();
        execution.Invoke(TGuarded<TCreature>(creature), &TCreature::ReceiveDamage, EDamageType::Fire, 100);
        execution.Invoke(TGuarded<TCreature>(creature), &TCreature::ReceiveDamage, EDamageType::Slashing, 10);
        Damage(creature, 0, true);
    }, {});
}
TEST_F(HealthRulesTest, DefensesApplyOnceBeforeTemporaryHitpointsAndNativeResolvedAdapter)
{
    auto values = HealthValues(EZeroHpRule::Dying, 8, 20, 2);
    values.Resistances = std::map<EDamageType, int>{{EDamageType::Slashing, 4}};
    values.Vulnerabilities = std::map<EDamageType, int>{{EDamageType::Slashing, 3}};
    auto generated = TCreature::Create(values);
    auto native = TCreature::Create(values);
    auto execution = journal_.ExecutionContext();
    const auto result = execution.Invoke(TGuarded<TCreature>(generated),
        &TCreature::ReceiveDamageWithContext, EDamageType::Slashing, 10,
        TDamageContext::Create({}));
    TPlayer player(&native, TPlayerTeam{0}, TPlayerId{0}, "Native", "");
    journal_.DealDamage(&player, 9);
    EXPECT_EQ(result.Hitpoints().Incoming(), 9);
    EXPECT_EQ(result.Hitpoints().AbsorbedTemporary(), 2);
    EXPECT_EQ(result.Hitpoints().After(), 1);
    EXPECT_FALSE(result.NewlyKnockedOut());
    EXPECT_EQ(native.Hitpoints()->Current(), 1);
    EXPECT_EQ(native.DyingValue(), 0);
}

TEST_F(HealthRulesTest, NonlethalKnockoutPreservesLifeUnderBothStrictPolicies)
{
    for (const auto policy : {EZeroHpRule::Dying, EZeroHpRule::Death}) {
        auto creature = TCreature::Create(HealthValues(policy, 8, 20, 0, 0, 1));
        std::optional<THealthDamageResult> result;
        Step(creature, [&] { result.emplace(Damage(creature, 8, true, true)); },
             {{"hitpoints.current", 0}, {"conditions.Unconscious", true}, {"conditions.Prone", true}});
        EXPECT_TRUE(result->NewlyKnockedOut());
        EXPECT_FALSE(result->NewlyDead());
        if (policy == EZeroHpRule::Dying) {
            Step(creature, [&] { Damage(creature, 1, true); },
                 {{"conditions.Dying", true}, {"conditions.Dying.value", 3}});
        } else {
            Step(creature, [&] { Damage(creature, 1, true); },
                 {{"dead", true}, {"actions.count", 0}, {"reactions.count", 0}});
        }
    }
}
TEST_F(HealthRulesTest, MassiveDamageUsesOneResolvedBlowAndCheckedThreshold)
{
    auto below = TCreature::Create(HealthValues(EZeroHpRule::Dying, 20));
    Damage(below, 39);
    EXPECT_FALSE(below.IsDead());
    EXPECT_EQ(below.DyingValue(), 1);
    auto massive = TCreature::Create(HealthValues(EZeroHpRule::Dying, 20, 20, 100));
    const auto result = Damage(massive, 40, false, true);
    EXPECT_EQ(result.Hitpoints().AbsorbedTemporary(), 40);
    EXPECT_EQ(result.Hitpoints().After(), 0);
    EXPECT_TRUE(result.NewlyDead());
    EXPECT_TRUE(massive.IsDead());
    auto enormous = TCreature::Create(HealthValues(EZeroHpRule::Dying,
        std::numeric_limits<int>::max(), std::numeric_limits<int>::max()));
    EXPECT_FALSE(enormous.IsMassiveDamage(std::numeric_limits<int>::max()));
    EXPECT_NO_THROW(Damage(enormous, std::numeric_limits<int>::max()));
    EXPECT_FALSE(enormous.IsDead());
    EXPECT_EQ(enormous.DyingValue(), 1);
}

TEST_F(HealthRulesTest, AllRecoveryDegreesApply)
{
    struct TCase {
        int Natural;
        ECheckOutcome Outcome;
        std::map<std::string, TField> Changes;
    };
    const std::vector<TCase> cases = {
        {20, ECheckOutcome::CriticalSuccess, {{"conditions.Dying", "<removed>"}, {"conditions.Dying.value", "<removed>"},
                                               {"conditions.Wounded", true}, {"conditions.Wounded.value", 1}}},
        {12, ECheckOutcome::Success, {{"conditions.Dying.value", 1}}},
        {11, ECheckOutcome::Failure, {{"conditions.Dying.value", 3}}},
        {1, ECheckOutcome::CriticalFailure, {{"conditions.Dying.value", 4}, {"dead", true}, {"actions.count", 0},
                                              {"reactions.count", 0}}},
    };
    for (const auto& sample : cases) {
        SCOPED_TRACE(sample.Natural);
        auto creature = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 2));
        Step(creature, [&] { EXPECT_EQ(Recover(creature, sample.Natural), sample.Outcome); }, sample.Changes);
    }
}
TEST_F(HealthRulesTest, RecoveryDcAdjustmentChangesTheOutcomeWithoutChangingStoredProfiles)
{
    auto ordinary = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 1));
    auto adjusted = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 1));
    const auto before = ObjectFields(adjusted);
    EXPECT_EQ(adjusted.RecoveryDC(0), 11);
    EXPECT_EQ(adjusted.RecoveryDC(-1), 10);
    EXPECT_EQ(ObjectFields(adjusted), before);
    EXPECT_EQ(Recover(ordinary, 10), ECheckOutcome::Failure);
    EXPECT_EQ(Recover(adjusted, 10, -1), ECheckOutcome::Success);
    EXPECT_EQ(ordinary.DyingValue(), 2);
    EXPECT_EQ(adjusted.DyingValue(), 0);
    EXPECT_EQ(adjusted.WoundedValue(), 1);
    EXPECT_FALSE(adjusted.NeedsRecoveryCheck());
    EXPECT_FALSE(adjusted.CanAct());
}

TEST_F(HealthRulesTest, NaturalOneAndTwentyChangeOneDegreeRatherThanForceAnOutcome)
{
    auto easy = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 2));
    auto hard = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 2));
    EXPECT_EQ(Recover(easy, 1, -22), ECheckOutcome::Success);
    EXPECT_EQ(easy.DyingValue(), 1);
    EXPECT_EQ(Recover(hard, 20, 33), ECheckOutcome::Failure);
    EXPECT_EQ(hard.DyingValue(), 3);
}

TEST_F(HealthRulesTest, HealingLosesDyingOnceLeavesProneAndDoesNotRestoreTemporaryHitpoints)
{
    auto creature = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 3, 2, 1));
    Step(creature, [&] { EXPECT_EQ(Heal(creature, 0), 0); }, {});
    Step(creature, [&] { EXPECT_EQ(Heal(creature, 5), 5); }, {
        {"hitpoints.current", 5}, {"conditions.Dying", "<removed>"}, {"conditions.Dying.value", "<removed>"},
        {"conditions.Unconscious", "<removed>"}, {"conditions.Wounded.value", 2}});
    Step(creature, [&] { EXPECT_EQ(Heal(creature, 100), 15); }, {{"hitpoints.current", 20}});
}
TEST_F(HealthRulesTest, StabilizingThenHealingDoesNotIncreaseWoundedTwice)
{
    auto creature = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 1));
    EXPECT_EQ(Recover(creature, 20), ECheckOutcome::CriticalSuccess);
    EXPECT_EQ(creature.WoundedValue(), 1);
    EXPECT_EQ(Heal(creature, 3), 3);
    EXPECT_EQ(creature.WoundedValue(), 1);
    Damage(creature, 3, true);
    EXPECT_EQ(creature.DyingValue(), 3);
    EXPECT_EQ(creature.WoundedValue(), 1);
    Damage(creature, 1, true);
    EXPECT_TRUE(creature.IsDead());
}

TEST_F(HealthRulesTest, DeathCannotBeHealedButLegacyDefeatRetainsItsImportBehavior)
{
    auto legacy = TCreature::Create(HealthValues(EZeroHpRule::Defeat, 8));
    auto monster = TCreature::Create(HealthValues(EZeroHpRule::Death, 8));
    Damage(legacy, 100);
    const auto killed = Damage(monster, 8);
    EXPECT_FALSE(legacy.IsDead());
    EXPECT_FALSE(legacy.IsAlive());
    EXPECT_FALSE(legacy.Conditions()->Has(EConditionKind::Unconscious));
    EXPECT_TRUE(killed.NewlyDead());
    EXPECT_FALSE(monster.NeedsRecoveryCheck());
    const auto dead_snapshot = StateSnapshot(monster, journal_);
    EXPECT_THROW(Heal(monster, 20), std::exception);
    EXPECT_THROW(Damage(monster, 1), std::exception);
    ExpectSame(dead_snapshot, StateSnapshot(monster, journal_), "dead creature");
    EXPECT_EQ(Heal(legacy, 20), 20);
    EXPECT_TRUE(legacy.CanAct());
}

TEST_F(HealthRulesTest, FailedKnockoutOrHealingRestoresNestedHitpointAndConditionWrites)
{
    auto damaged = TCreature::Create(HealthValues(EZeroHpRule::Dying, 8, 20, 0, 0,
                                                  std::numeric_limits<int>::max()));
    const auto before_damage = StateSnapshot(damaged, journal_);
    EXPECT_THROW(Damage(damaged, 8), std::overflow_error);
    EXPECT_EQ(StateSnapshot(damaged, journal_), before_damage);
    auto healed = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 1,
                                                 std::numeric_limits<int>::max()));
    const auto before_healing = StateSnapshot(healed, journal_);
    EXPECT_THROW(Heal(healed, 10), std::overflow_error);
    EXPECT_EQ(StateSnapshot(healed, journal_), before_healing);
}

TEST_F(HealthRulesTest, InvalidRecoveryInputsAndDcOverflowLeaveStateAndJournalUnchanged)
{
    auto creature = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 1));
    const auto before = StateSnapshot(creature, journal_);
    EXPECT_THROW(Recover(creature, 0), std::exception);
    EXPECT_THROW(Recover(creature, 21), std::exception);
    EXPECT_THROW(Recover(creature, 10, std::numeric_limits<int>::max()), std::overflow_error);
    EXPECT_THROW(Damage(creature, -1), std::exception);
    EXPECT_THROW(Heal(creature, -1), std::exception);
    EXPECT_EQ(StateSnapshot(creature, journal_), before);
    auto stable = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 0, 1, true));
    const auto stable_before = StateSnapshot(stable, journal_);
    EXPECT_THROW(Recover(stable, 20), std::exception);
    EXPECT_EQ(StateSnapshot(stable, journal_), stable_before);
}

TEST_F(HealthRulesTest, NativeDamageHealingAndConditionAdaptersUseTheJournal)
{
    auto creature = TCreature::Create(HealthValues(EZeroHpRule::Dying, 8));
    TPlayer player(&creature, TPlayerTeam{0}, TPlayerId{0}, "Patient", "");
    Step(creature, [&] {
        EXPECT_TRUE(journal_.DealDamageWithContext(&player, 8, TDamageContext::Create({.Critical = true})).NewlyKnockedOut());
        EXPECT_EQ(creature.DyingValue(), 2);
        journal_.Heal(&player, 3);
        SetCondition(journal_, creature, TConditionUnconscious{});
        SetCondition(journal_, creature, TConditionWounded{.Value = 2});
        SetCondition(journal_, creature, TConditionDying{.Value = 1});
    }, {{"hitpoints.current", 3}, {"conditions.Unconscious", true}, {"conditions.Prone", true},
        {"conditions.Wounded", true}, {"conditions.Wounded.value", 2}, {"conditions.Dying", true},
        {"conditions.Dying.value", 1}});
    EXPECT_THROW(journal_.DealDamage(nullptr, 1), std::invalid_argument);
    EXPECT_THROW(journal_.Heal(nullptr, 1), std::invalid_argument);
}
TEST_F(HealthRulesTest, SleepingCreaturesWakeAfterPositiveDamageOrHealing)
{
    auto damaged = TCreature::Create(HealthValues(EZeroHpRule::Dying, 8, 20, 0, 0, 0, true));
    auto healed = TCreature::Create(HealthValues(EZeroHpRule::Dying, 20, 20, 0, 0, 0, true));
    EXPECT_FALSE(damaged.CanAct());
    EXPECT_FALSE(healed.CanAct());
    Damage(damaged, 1);
    EXPECT_TRUE(damaged.CanAct());
    EXPECT_EQ(Heal(healed, 1), 0);
    EXPECT_TRUE(healed.CanAct());
}

TEST_F(HealthRulesTest, UnconsciousCreaturesRegainTurnResourcesWhileDeadCreaturesDoNot)
{
    auto unconscious = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 1));
    auto dead = TCreature::Create(HealthValues(EZeroHpRule::Death, 0));
    auto execution = journal_.ExecutionContext();
    execution.Invoke(TGuarded<TCreature>(unconscious), &TCreature::BeginTurn);
    execution.Invoke(TGuarded<TCreature>(dead), &TCreature::BeginTurn);
    EXPECT_EQ(unconscious.Actions()->Count(), 3);
    EXPECT_EQ(unconscious.Reactions()->Count(), 1);
    EXPECT_FALSE(unconscious.CanAct());
    EXPECT_EQ(dead.Actions()->Count(), 0);
    EXPECT_EQ(dead.Reactions()->Count(), 0);
    EXPECT_FALSE(dead.CanAct());
}

}

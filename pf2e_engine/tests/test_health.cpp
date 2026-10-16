#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/mechanics/creature.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

#include "test_lib/mock_interaction_system.h"

#include <cpp_config.h>
#include <gtest/gtest.h>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

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

TAstNode Snapshot(const TCreature& creature, const TTransformator& journal)
{
    TAstContext context;
    context.RegisterIdentity(&creature, "health_creature");
    auto node = TAstNode::MakeObject("health");
    AddOwnedObject(node, "creature", creature, context);
    AddOwnedObject(node, "journal", journal, context);
    return node;
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
        const auto checkpoint = journal_.CurrentState();
        const auto before = Snapshot(creature, journal_);
        const auto result = Damage(creature, 8, critical);
        EXPECT_EQ(result.Hitpoints().Before(), 8);
        EXPECT_EQ(result.Hitpoints().After(), 0);
        EXPECT_TRUE(result.NewlyKnockedOut());
        EXPECT_FALSE(result.NewlyDead());
        EXPECT_EQ(creature.DyingValue(), critical ? 3 : 2);
        EXPECT_EQ(creature.WoundedValue(), 1);
        EXPECT_TRUE(creature.Conditions()->Has(EConditionKind::Unconscious));
        EXPECT_TRUE(creature.Conditions()->Has(EConditionKind::Prone));
        EXPECT_TRUE(creature.IsAlive());
        EXPECT_FALSE(creature.CanAct());
        EXPECT_EQ(creature.Actions()->Count(), 2);
        EXPECT_EQ(creature.Reactions()->Count(), 1);
        journal_.Undo(checkpoint);
        EXPECT_EQ(Snapshot(creature, journal_), before);
    }
}

TEST_F(HealthRulesTest, DamageWhileDyingIncludesTemporaryAbsorptionAndDoesNotReaddWounded)
{
    auto creature = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 5, 1, 1));
    const auto checkpoint = journal_.CurrentState();
    const auto before = Snapshot(creature, journal_);
    const auto critical = Damage(creature, 1, true);
    EXPECT_EQ(critical.Hitpoints().AbsorbedTemporary(), 1);
    EXPECT_EQ(critical.Hitpoints().LostCurrent(), 0);
    EXPECT_FALSE(critical.NewlyKnockedOut());
    EXPECT_EQ(creature.DyingValue(), 3);
    EXPECT_EQ(creature.WoundedValue(), 1);
    const auto nonlethal = Damage(creature, 1, false, true);
    EXPECT_TRUE(nonlethal.NewlyDead());
    EXPECT_EQ(creature.DyingValue(), 4);
    EXPECT_EQ(creature.Hitpoints()->Temporary(), 3);
    EXPECT_TRUE(creature.IsDead());
    journal_.Undo(checkpoint);
    EXPECT_EQ(Snapshot(creature, journal_), before);
}

TEST_F(HealthRulesTest, PreventedDamageCannotIncreaseDying)
{
    auto values = HealthValues(EZeroHpRule::Dying, 0, 20, 3, 1);
    values.Immunities = std::set<EDamageType>{EDamageType::Fire};
    values.Resistances = std::map<EDamageType, int>{{EDamageType::Slashing, 10}};
    auto creature = TCreature::Create(values);
    const auto before = creature.ToJson();
    auto execution = journal_.ExecutionContext();
    execution.Invoke(TGuarded<TCreature>(creature), &TCreature::ReceiveDamage,
                     EDamageType::Fire, 100);
    execution.Invoke(TGuarded<TCreature>(creature), &TCreature::ReceiveDamage,
                     EDamageType::Slashing, 10);
    Damage(creature, 0, true);
    EXPECT_EQ(creature.ToJson(), before);
    EXPECT_EQ(creature.DyingValue(), 1);
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
        const auto checkpoint = journal_.CurrentState();
        const auto before = Snapshot(creature, journal_);
        const auto result = Damage(creature, 8, true, true);
        EXPECT_TRUE(result.NewlyKnockedOut());
        EXPECT_FALSE(result.NewlyDead());
        EXPECT_TRUE(creature.IsAlive());
        EXPECT_FALSE(creature.NeedsRecoveryCheck());
        EXPECT_EQ(creature.DyingValue(), 0);
        EXPECT_EQ(creature.WoundedValue(), 1);
        Damage(creature, 1, true);
        if (policy == EZeroHpRule::Dying) {
            EXPECT_EQ(creature.DyingValue(), 3);
            EXPECT_TRUE(creature.NeedsRecoveryCheck());
        } else {
            EXPECT_TRUE(creature.IsDead());
        }
        journal_.Undo(checkpoint);
        EXPECT_EQ(Snapshot(creature, journal_), before);
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

TEST_F(HealthRulesTest, AllRecoveryDegreesApplyAndRollbackIncludingNaturalAdjustment)
{
    struct TCase {
        int Natural;
        ECheckOutcome Outcome;
        int Dying;
        int Wounded;
        bool Dead;
    };
    const std::array cases = {
        TCase{20, ECheckOutcome::CriticalSuccess, 0, 1, false},
        TCase{12, ECheckOutcome::Success, 1, 0, false},
        TCase{11, ECheckOutcome::Failure, 3, 0, false},
        TCase{1, ECheckOutcome::CriticalFailure, 4, 0, true},
    };
    for (const auto& sample : cases) {
        auto creature = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 2));
        const auto checkpoint = journal_.CurrentState();
        const auto before = Snapshot(creature, journal_);
        EXPECT_EQ(Recover(creature, sample.Natural), sample.Outcome);
        EXPECT_EQ(creature.DyingValue(), sample.Dying);
        EXPECT_EQ(creature.WoundedValue(), sample.Wounded);
        EXPECT_EQ(creature.IsDead(), sample.Dead);
        EXPECT_EQ(creature.Hitpoints()->Current(), 0);
        EXPECT_TRUE(creature.Conditions()->Has(EConditionKind::Unconscious));
        EXPECT_FALSE(creature.CanAct());
        journal_.Undo(checkpoint);
        EXPECT_EQ(Snapshot(creature, journal_), before);
    }
}

TEST_F(HealthRulesTest, RecoveryDcAdjustmentChangesTheOutcomeWithoutChangingStoredProfiles)
{
    auto ordinary = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 1));
    auto adjusted = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 1));
    const auto before = adjusted.ToJson();
    EXPECT_EQ(adjusted.RecoveryDC(0), 11);
    EXPECT_EQ(adjusted.RecoveryDC(-1), 10);
    EXPECT_EQ(adjusted.ToJson(), before);
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
    const auto checkpoint = journal_.CurrentState();
    const auto before = Snapshot(creature, journal_);
    EXPECT_EQ(Heal(creature, 0), 0);
    EXPECT_EQ(creature.DyingValue(), 2);
    EXPECT_EQ(Heal(creature, 5), 5);
    EXPECT_EQ(creature.DyingValue(), 0);
    EXPECT_EQ(creature.WoundedValue(), 2);
    EXPECT_FALSE(creature.Conditions()->Has(EConditionKind::Unconscious));
    EXPECT_TRUE(creature.Conditions()->Has(EConditionKind::Prone));
    EXPECT_TRUE(creature.CanAct());
    EXPECT_EQ(Heal(creature, 100), 15);
    EXPECT_EQ(creature.WoundedValue(), 2);
    EXPECT_EQ(creature.Hitpoints()->Temporary(), 3);
    journal_.Undo(checkpoint);
    EXPECT_EQ(Snapshot(creature, journal_), before);
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
    const auto dead_snapshot = Snapshot(monster, journal_);
    EXPECT_THROW(Heal(monster, 20), std::exception);
    EXPECT_THROW(Damage(monster, 1), std::exception);
    EXPECT_EQ(Snapshot(monster, journal_), dead_snapshot);
    EXPECT_EQ(Heal(legacy, 20), 20);
    EXPECT_TRUE(legacy.CanAct());
    TGameObjectFactory factory;
    const auto restored = TCreature::Restore(monster.ToJson(), factory);
    EXPECT_EQ(restored.ToJson(), monster.ToJson());
    EXPECT_TRUE(restored.IsDead());
}

TEST_F(HealthRulesTest, FailedKnockoutOrHealingRestoresNestedHitpointAndConditionWrites)
{
    auto damaged = TCreature::Create(HealthValues(EZeroHpRule::Dying, 8, 20, 0, 0,
                                                  std::numeric_limits<int>::max()));
    const auto before_damage = Snapshot(damaged, journal_);
    EXPECT_THROW(Damage(damaged, 8), std::overflow_error);
    EXPECT_EQ(Snapshot(damaged, journal_), before_damage);
    auto healed = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 1,
                                                 std::numeric_limits<int>::max()));
    const auto before_healing = Snapshot(healed, journal_);
    EXPECT_THROW(Heal(healed, 10), std::overflow_error);
    EXPECT_EQ(Snapshot(healed, journal_), before_healing);
}

TEST_F(HealthRulesTest, InvalidRecoveryInputsAndDcOverflowLeaveStateAndJournalUnchanged)
{
    auto creature = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 1));
    const auto before = Snapshot(creature, journal_);
    EXPECT_THROW(Recover(creature, 0), std::exception);
    EXPECT_THROW(Recover(creature, 21), std::exception);
    EXPECT_THROW(Recover(creature, 10, std::numeric_limits<int>::max()), std::overflow_error);
    EXPECT_THROW(Damage(creature, -1), std::exception);
    EXPECT_THROW(Heal(creature, -1), std::exception);
    EXPECT_EQ(Snapshot(creature, journal_), before);
    auto stable = TCreature::Create(HealthValues(EZeroHpRule::Dying, 0, 20, 0, 0, 1, true));
    const auto stable_before = Snapshot(stable, journal_);
    EXPECT_THROW(Recover(stable, 20), std::exception);
    EXPECT_EQ(Snapshot(stable, journal_), stable_before);
}

TEST_F(HealthRulesTest, NativeDamageHealingAndConditionAdaptersShareExactUndoAndReplay)
{
    auto creature = TCreature::Create(HealthValues(EZeroHpRule::Dying, 8));
    TPlayer player(&creature, TPlayerTeam{0}, TPlayerId{0}, "Patient", "");
    const auto checkpoint = journal_.CurrentState();
    const auto before = Snapshot(creature, journal_);
    nlohmann::json first;
    for (int repeat = 0; repeat < 2; ++repeat) {
        const auto result = journal_.DealDamageWithContext(&player, 8,
            TDamageContext::Create({.Critical = true}));
        EXPECT_EQ(creature.DyingValue(), 2);
        EXPECT_TRUE(result.NewlyKnockedOut());
        journal_.Heal(&player, 3);
        EXPECT_EQ(creature.Hitpoints()->Current(), 3);
        EXPECT_EQ(creature.WoundedValue(), 1);
        journal_.ChangeCondition(&creature, EConditionKind::Unconscious, 1);
        journal_.ChangeCondition(&creature, EConditionKind::Wounded, 2);
        journal_.ChangeCondition(&creature, EConditionKind::Dying, 1);
        EXPECT_EQ(ConditionValue(creature, EConditionKind::Unconscious), 1);
        EXPECT_EQ(ConditionValue(creature, EConditionKind::Wounded), 2);
        EXPECT_EQ(ConditionValue(creature, EConditionKind::Dying), 1);
        if (repeat == 0) {
            first = creature.ToJson();
        } else {
            EXPECT_EQ(creature.ToJson(), first);
        }
        journal_.Undo(checkpoint);
        EXPECT_EQ(Snapshot(creature, journal_), before);
    }
    EXPECT_THROW(journal_.DealDamage(nullptr, 1), std::invalid_argument);
    EXPECT_THROW(journal_.Heal(nullptr, 1), std::invalid_argument);
    EXPECT_EQ(Snapshot(creature, journal_), before);
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

TEST_F(HealthRulesTest, JsonImportAndSnapshotsPreservePolicyAndHealthConditions)
{
    TGameObjectFactory factory;
    const auto creature = TCreature::FromJson({
        {"zero_hp_rule", "Dying"},
        {"hitpoints", {{"maximum", {{"base", 20}}}, {"current", 0}}},
        {"conditions", nlohmann::json::array({{{"Dying", 2}}, {{"Wounded", 1}}, "Unconscious", "Prone"})},
    }, factory);
    EXPECT_EQ(creature.DyingValue(), 2);
    EXPECT_EQ(creature.WoundedValue(), 1);
    EXPECT_TRUE(creature.NeedsRecoveryCheck());
    const auto json = creature.ToJson();
    const auto restored = TCreature::Restore(json, factory);
    EXPECT_EQ(restored.ToJson(), json);
    TAstContext first;
    TAstContext second;
    EXPECT_EQ(creature.GetAst(first), restored.GetAst(second));
    EXPECT_THROW(TCreature::FromJson({{"zero_hp_rule", "Unknown"}}, factory), std::exception);
    EXPECT_THROW(TCreature::FromJson({{"conditions", nlohmann::json::array({
        {{"Dying", 2147483648LL}},
    })}}, factory), std::exception);

    std::ifstream input(kRootDirPath + "/pf2e_engine/schemas/schema.json");
    const auto schema = nlohmann::json::parse(input);
    nlohmann::json_schema::json_validator validator({
        {"$ref", "#/$defs/pf2e_creature_data_core"}, {"$defs", schema.at("$defs")},
    });
    nlohmann::json profile = {
        {"level", 1}, {"size", "Medium"}, {"race", "human"}, {"class", "fighter"},
        {"characteristic", "scores"}, {"armor", "unarmored"},
        {"zero_hp_rule", "Dying"}, {"dead", false},
        {"conditions", nlohmann::json::array({{{"Dying", 2}}, {{"Wounded", 1}}, "Unconscious"})},
    };
    EXPECT_NO_THROW(validator.validate(profile));
    profile["conditions"] = nlohmann::json::array({{{"Dying", -1}}});
    EXPECT_THROW(validator.validate(profile), std::exception);
    profile["conditions"] = nlohmann::json::array({{{"Wounded", 0}}});
    EXPECT_THROW(validator.validate(profile), std::exception);
    profile["conditions"] = nlohmann::json::array();
    profile["zero_hp_rule"] = "Unknown";
    EXPECT_THROW(validator.validate(profile), std::exception);
}

}

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/inventory/combat_checks.h>
#include <pf2e_engine/inventory/statistic_baseline.h>
#include <pf2e_engine/transformation/transformator.h>

#include "../test_lib/mock_interaction_system.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <limits>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace {

TStatisticBaseline FixedBaseline(int value)
{
    return TStatisticBaseline::Create({.IsFixed = true, .Value = value});
}

TAbilityScores Scores(int strength, int dexterity)
{
    TGameObjectFactory factory;
    return TAbilityScores::FromJson({
        {"strength", {{"value", strength}}},
        {"dexterity", {{"value", dexterity}}},
        {"constitution", {{"value", 14}}},
        {"intelligence", {{"value", 10}}},
        {"wisdom", {{"value", 12}}},
        {"charisma", {{"value", 10}}},
    }, factory);
}

TCreature Fighter()
{
    TCreature::TInitialValues values;
    values.Level = 1;
    values.Characteristic = Scores(18, 14);
    values.Proficiency = TProficiency::Create({
        .ArmorCategory = std::map<EArmorCategory, TProficiencyValue>{
            {EArmorCategory::Unarmored, TProficiencyValue::Create({.Rank = EProficiencyRank::Trained})}},
        .WeaponCategory = std::map<EWeaponCategory, TProficiencyValue>{
            {EWeaponCategory::Martial, TProficiencyValue::Create({.Rank = EProficiencyRank::Expert})}},
    });
    values.Hitpoints = THitPoints::Create({
        .Maximum = Statistic::Create({.Base = 21}), .Current = 21,
    });
    return TCreature::Create(std::move(values));
}

TWeapon Sword(bool agile = false)
{
    TWeapon::TInitialValues values;
    values.Name = "Sword";
    values.BaseDiceSize = 8;
    values.DamageType = EDamageType::Slashing;
    values.Category = EWeaponCategory::Martial;
    if (agile) {
        TVariantMap<EWeaponTraitKind, TWeaponTrait> traits;
        traits.Set(TWeaponTrait(TWeaponTraitAgile{}));
        values.Traits = std::move(traits);
    }
    return TWeapon::Create(std::move(values));
}

TCreature Npc()
{
    TCreature::TInitialValues values;
    values.Level = 1;
    values.Characteristic = Scores(14, 18);
    values.ArmorClassBaseline = FixedBaseline(15);
    return TCreature::Create(std::move(values));
}

TWeapon Jaws(int attack = 9)
{
    TWeapon::TInitialValues values;
    values.Name = "Jaws";
    values.BaseDiceSize = 6;
    values.DamageType = EDamageType::Piercing;
    values.AttackBonusBaseline = FixedBaseline(attack);
    values.DamageModifierBaseline = FixedBaseline(2);
    return TWeapon::Create(std::move(values));
}

TAstNode Snapshot(const TCreature& creature, const TTransformator& journal)
{
    TAstContext context;
    context.RegisterIdentity(&creature, "creature");
    auto node = TAstNode::MakeObject("combat_rules");
    AddOwnedObject(node, "journal", journal, context);
    AddOwnedObject(node, "creature", creature, context);
    return node;
}

TEST(GeneratedCombatRulesTest, ComputesPcStatisticsAndUsesCompleteNpcBaselines)
{
    const auto fighter = Fighter();
    const auto npc = Npc();
    const auto sword = Sword();
    const auto jaws = Jaws();
    EXPECT_EQ(fighter.ArmorClass(), 15);
    EXPECT_EQ(fighter.AttackBonus(sword), 9);
    EXPECT_EQ(fighter.DamageModifier(sword), 4);
    EXPECT_EQ(npc.ArmorClass(), 15);
    EXPECT_EQ(npc.AttackBonus(jaws), 9);
    EXPECT_EQ(npc.DamageModifier(jaws), 2);
    EXPECT_EQ(fighter.AttackBonus(jaws), 9);
    EXPECT_EQ(fighter.DamageModifier(jaws), 2);
    EXPECT_EQ(fighter.AttackBonus(Jaws(0)), 0);
    EXPECT_EQ(fighter.AttackBonus(Jaws(-3)), -3);
}

TEST(GeneratedCombatRulesTest, FixedProficiencyRemainsAContributionAndLevelChangesAreLive)
{
    TMockInteractionSystem io;
    TTransformator journal(io);
    auto context = journal.ExecutionContext();
    auto fighter = Fighter();
    const auto sword = Sword();
    const auto before = Snapshot(fighter, journal);
    const auto start = context.GetState();
    context.AssignGuarded(fighter.Level(), 10, TGuarded<TCreature>(fighter), "level");
    EXPECT_EQ(fighter.AttackBonus(sword), 18);
    EXPECT_EQ(fighter.AttackBonus(Jaws()), 9);
    const auto fixed = TProficiency::Create({
        .WeaponCategory = std::map<EWeaponCategory, TProficiencyValue>{{
            EWeaponCategory::Martial,
            TProficiencyValue::Create({.IsFixed = true, .FixedBonus = 7}),
        }},
    });
    context.AssignGuarded(fighter.Proficiency(), fixed, TGuarded<TCreature>(fighter), "proficiency");
    EXPECT_EQ(fighter.AttackBonus(sword), 11);
    EXPECT_EQ(fighter.AttackBonus(Jaws()), 9);
    context.Undo(start);
    EXPECT_EQ(fighter.AttackBonus(sword), 9);
    EXPECT_EQ(Snapshot(fighter, journal), before);
}

TEST(GeneratedCombatRulesTest, ConditionsApplyToBothComputedAndFixedStatistics)
{
    TMockInteractionSystem io;
    TTransformator journal(io);
    auto fighter = Fighter();
    auto npc = Npc();
    const auto start = journal.CurrentState();
    for (auto* creature : {&fighter, &npc}) {
        journal.ChangeCondition(creature, EConditionKind::Frightened, 2);
        journal.ChangeCondition(creature, EConditionKind::Prone, 1);
        EXPECT_EQ(creature->ArmorClass(), 11);
        EXPECT_EQ(creature->AttackBonus(Jaws()), 5);
    }
    EXPECT_EQ(fighter.AttackBonus(Sword()), 5);
    EXPECT_EQ(fighter.DamageModifier(Sword()), 4);
    journal.Undo(start);
    EXPECT_EQ(fighter.ArmorClass(), 15);
    EXPECT_EQ(npc.AttackBonus(Jaws()), 9);
}

TEST(GeneratedCombatRulesTest, CurrentWeaponDeterminesPenaltyAndAttackHistoryRollsBack)
{
    TMockInteractionSystem io;
    TTransformator journal(io);
    auto context = journal.ExecutionContext();
    auto fighter = Fighter();
    const auto sword = Sword();
    const auto agile = Sword(true);
    const auto before = Snapshot(fighter, journal);
    const auto start = context.GetState();
    EXPECT_EQ(fighter.MultipleAttackPenalty(sword), 0);
    EXPECT_EQ(fighter.MultipleAttackPenalty(agile), 0);
    context.Invoke(TGuarded<TCreature>(fighter), &TCreature::RecordAttack, 1);
    EXPECT_EQ(fighter.AttacksMade().Get(), 1);
    EXPECT_EQ(fighter.MultipleAttackPenalty(sword), 5);
    EXPECT_EQ(fighter.MultipleAttackPenalty(agile), 4);
    const auto after_first = context.GetState();
    context.Invoke(TGuarded<TCreature>(fighter), &TCreature::RecordAttack, 1);
    EXPECT_EQ(fighter.MultipleAttackPenalty(sword), 10);
    EXPECT_EQ(fighter.MultipleAttackPenalty(agile), 8);
    context.Invoke(TGuarded<TCreature>(fighter), &TCreature::RecordAttack, 2);
    EXPECT_EQ(fighter.AttacksMade().Get(), 4);
    EXPECT_EQ(fighter.AttackCountPenalty(false), 10);
    EXPECT_EQ(fighter.MultipleAttackPenalty(agile), 8);
    context.Invoke(TGuarded<TCreature>(fighter), &TCreature::ResetAttacks);
    EXPECT_EQ(fighter.MultipleAttackPenalty(sword), 0);
    context.Undo(after_first);
    EXPECT_EQ(fighter.AttacksMade().Get(), 1);
    context.Undo(start);
    EXPECT_EQ(Snapshot(fighter, journal), before);
}

TEST(GeneratedCombatRulesTest, InvalidAttackCountsAndOverflowLeaveTheJournalUnchanged)
{
    TMockInteractionSystem io;
    TTransformator journal(io);
    auto context = journal.ExecutionContext();
    auto fighter = Fighter();
    const auto before = Snapshot(fighter, journal);
    for (const int count : {0, -1}) {
        EXPECT_THROW(context.Invoke(TGuarded<TCreature>(fighter), &TCreature::RecordAttack, count),
                     std::runtime_error);
        EXPECT_EQ(Snapshot(fighter, journal), before);
    }
    context.AssignGuarded(fighter.AttacksMade(), std::numeric_limits<int>::max(),
                          TGuarded<TCreature>(fighter), "attacks_made");
    const auto at_limit = Snapshot(fighter, journal);
    EXPECT_THROW(context.Invoke(TGuarded<TCreature>(fighter), &TCreature::RecordAttack, 1),
                 std::overflow_error);
    EXPECT_EQ(Snapshot(fighter, journal), at_limit);
}

TEST(GeneratedCombatRulesTest, DamageDefenseOrderPreservesZeroAndTemporaryHpRollback)
{
    TMockInteractionSystem io;
    TTransformator journal(io);
    auto context = journal.ExecutionContext();
    TCreature::TInitialValues values;
    values.Hitpoints = THitPoints::Create({
        .Maximum = Statistic::Create({.Base = 21}), .Current = 21, .Temporary = 3,
    });
    values.Immunities = std::set<EDamageType>{EDamageType::Fire};
    values.Resistances = std::map<EDamageType, int>{{EDamageType::Piercing, 3}};
    values.Vulnerabilities = std::map<EDamageType, int>{
        {EDamageType::Piercing, 2}, {EDamageType::Fire, 10},
    };
    auto creature = TCreature::Create(std::move(values));
    EXPECT_EQ(creature.DamageAmount(EDamageType::Fire, 20), 0);
    EXPECT_EQ(creature.DamageAmount(EDamageType::Piercing, 0), 0);
    EXPECT_EQ(creature.DamageAmount(EDamageType::Piercing, 1), 0);
    EXPECT_EQ(creature.DamageAmount(EDamageType::Piercing, 5), 4);
    const auto before = Snapshot(creature, journal);
    const auto start = context.GetState();
    context.Invoke(TGuarded<TCreature>(creature), &TCreature::ReceiveDamage,
                    EDamageType::Piercing, 5);
    EXPECT_EQ(creature.Hitpoints()->Temporary(), 0);
    EXPECT_EQ(creature.Hitpoints()->Current(), 20);
    EXPECT_THROW(context.Invoke(TGuarded<TCreature>(creature), &TCreature::ReceiveDamage,
                                 EDamageType::Piercing, -1), std::runtime_error);
    EXPECT_EQ(creature.Hitpoints()->Current(), 20);
    context.Undo(start);
    EXPECT_EQ(Snapshot(creature, journal), before);
}

TEST(GeneratedCombatRulesTest, CheckBoundariesAndNaturalDiceShiftOneDegree)
{
    const auto rules = TCheckRules::Create({});
    struct TCase {
        int Natural;
        int Bonus;
        int Dc;
        ECheckOutcome Expected;
    };
    const TCase cases[] = {
        {10, 9, 19, ECheckOutcome::Success},
        {10, 9, 20, ECheckOutcome::Failure},
        {10, 9, 9, ECheckOutcome::CriticalSuccess},
        {10, 9, 29, ECheckOutcome::CriticalFailure},
        {20, 0, 25, ECheckOutcome::Success},
        {20, 0, 30, ECheckOutcome::Failure},
        {20, 0, 10, ECheckOutcome::CriticalSuccess},
        {1, 30, 10, ECheckOutcome::Success},
        {1, 20, 20, ECheckOutcome::Failure},
        {1, 10, 20, ECheckOutcome::CriticalFailure},
    };
    for (const auto& entry : cases) {
        EXPECT_EQ(rules.Resolve(entry.Natural, entry.Bonus, entry.Dc), entry.Expected);
    }
    EXPECT_THROW(rules.Resolve(0, 0, 10), std::runtime_error);
    EXPECT_THROW(rules.Resolve(21, 0, 10), std::runtime_error);
    EXPECT_EQ(rules.MinimumDamage(-4), 1);
    EXPECT_EQ(rules.MinimumDamage(0), 1);
    EXPECT_EQ(rules.MinimumDamage(7), 7);
}

TEST(GeneratedCombatRulesTest, BaselineSnapshotsKeepExplicitZeroAndIndependentProfiles)
{
    TGameObjectFactory factory;
    const auto original = Jaws(0);
    const auto json = original.ToJson();
    const auto restored = TWeapon::Restore(json, factory);
    const auto fighter = Fighter();
    EXPECT_EQ(restored.ToJson(), json);
    EXPECT_TRUE(restored.AttackBonusBaseline().IsFixed());
    EXPECT_EQ(fighter.AttackBonus(restored), 0);
    EXPECT_EQ(fighter.AttackBonus(Sword()), 9);
    for (const auto& value : nlohmann::json::array({
             static_cast<std::int64_t>(std::numeric_limits<int>::max()) + 1,
             static_cast<std::int64_t>(std::numeric_limits<int>::min()) - 1,
             std::numeric_limits<std::uint64_t>::max(), 1.5, true,
         })) {
        auto invalid = json;
        invalid["attack_bonus_baseline"]["value"] = value;
        EXPECT_THROW(TWeapon::Restore(invalid, factory), std::exception);
    }
    for (const int value : {std::numeric_limits<int>::min(), -1, 0,
                            std::numeric_limits<int>::max()}) {
        auto valid = json;
        valid["attack_bonus_baseline"]["value"] = value;
        const auto weapon = TWeapon::Restore(valid, factory);
        EXPECT_EQ(weapon.AttackBonusBaseline().Value(), value);
        EXPECT_EQ(weapon.ToJson(), valid);
    }
}

}

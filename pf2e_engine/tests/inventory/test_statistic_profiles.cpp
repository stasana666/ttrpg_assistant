#include <pf2e_engine/creature.h>
#include <pf2e_engine/inventory/check_modifiers.h>
#include <pf2e_engine/inventory/lore.h>
#include <pf2e_engine/inventory/statistics.h>

#include "state_diff.h"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

TEST(StatisticProfilesTest, SkillAndSaveAttributesAreGenerated)
{
    const auto rules = TStatisticRules::Create({});
    const std::array skills{
        std::pair{ESkill::Acrobatics, ECharacteristic::Dexterity},
        std::pair{ESkill::Arcana, ECharacteristic::Intelligence},
        std::pair{ESkill::Athletics, ECharacteristic::Strength},
        std::pair{ESkill::Crafting, ECharacteristic::Intelligence},
        std::pair{ESkill::Deception, ECharacteristic::Charisma},
        std::pair{ESkill::Diplomacy, ECharacteristic::Charisma},
        std::pair{ESkill::Intimidation, ECharacteristic::Charisma},
        std::pair{ESkill::Medicine, ECharacteristic::Wisdom},
        std::pair{ESkill::Nature, ECharacteristic::Wisdom},
        std::pair{ESkill::Occultism, ECharacteristic::Intelligence},
        std::pair{ESkill::Performance, ECharacteristic::Charisma},
        std::pair{ESkill::Religion, ECharacteristic::Wisdom},
        std::pair{ESkill::Society, ECharacteristic::Intelligence},
        std::pair{ESkill::Stealth, ECharacteristic::Dexterity},
        std::pair{ESkill::Survival, ECharacteristic::Wisdom},
        std::pair{ESkill::Thievery, ECharacteristic::Dexterity},
    };
    for (const auto& [skill, attribute] : skills) {
        EXPECT_EQ(rules.SkillAttribute(skill), attribute);
    }
    for (const auto& [save, attribute] : std::array{
             std::pair{ESavethrow::Fortitude, ECharacteristic::Constitution},
             std::pair{ESavethrow::Reflex, ECharacteristic::Dexterity},
             std::pair{ESavethrow::Will, ECharacteristic::Wisdom}}) {
        EXPECT_EQ(rules.SaveAttribute(save), attribute);
    }
    EXPECT_EQ(rules.PerceptionAttribute(), ECharacteristic::Wisdom);
    EXPECT_THROW(rules.SkillAttribute(static_cast<ESkill>(100)), std::runtime_error);
    EXPECT_THROW(rules.SaveAttribute(static_cast<ESavethrow>(100)), std::runtime_error);
}

TEST(StatisticProfilesTest, ModifierMergeKeepsStrongestBonusAndPenaltyOfEachType)
{
    const auto first = TCheckModifiers::Create({
        .ItemBonus = 3, .ItemPenalty = 1, .StatusBonus = 1, .StatusPenalty = 4,
        .CircumstanceBonus = 2, .CircumstancePenalty = 1, .UntypedPenalty = 2,
    });
    const auto second = TCheckModifiers::Create({
        .ItemBonus = 1, .ItemPenalty = 2, .StatusBonus = 3, .StatusPenalty = 2,
        .CircumstanceBonus = 1, .CircumstancePenalty = 3, .UntypedPenalty = 1,
    });
    const auto first_fields = ObjectFields(first);
    const auto second_fields = ObjectFields(second);
    const auto merged = first.Merge(second);
    EXPECT_EQ(merged.ItemBonus(), 3);
    EXPECT_EQ(merged.ItemPenalty(), 2);
    EXPECT_EQ(merged.StatusBonus(), 3);
    EXPECT_EQ(merged.StatusPenalty(), 4);
    EXPECT_EQ(merged.CircumstanceBonus(), 2);
    EXPECT_EQ(merged.CircumstancePenalty(), 3);
    EXPECT_EQ(merged.UntypedPenalty(), 3);
    EXPECT_EQ(merged.Total(), -4);
    EXPECT_EQ(ObjectFields(merged), ObjectFields(second.Merge(first)));
    EXPECT_EQ(ObjectFields(first), first_fields);
    EXPECT_EQ(ObjectFields(second), second_fields);
    EXPECT_EQ(ObjectFields(first.Merge(TCheckModifiers::Create({}))), first_fields);
}

TEST(StatisticProfilesTest, ModifierMergingIsAssociativeAndUntypedPenaltiesAccumulate)
{
    const auto first = TCheckModifiers::Create({.StatusBonus = 2, .UntypedPenalty = 1});
    const auto second = TCheckModifiers::Create({.StatusBonus = 2, .UntypedPenalty = 3});
    const auto third = TCheckModifiers::Create({.StatusPenalty = 1, .CircumstanceBonus = 4});
    const auto result = first.Merge(second).Merge(third);
    EXPECT_EQ(ObjectFields(result), ObjectFields(first.Merge(second.Merge(third))));
    EXPECT_EQ(result.Total(), 1);
    EXPECT_EQ(first.Merge(first).StatusBonus(), 2);
    EXPECT_EQ(first.Merge(first).UntypedPenalty(), 2);
}

TEST(StatisticProfilesTest, InvalidModifiersAndArithmeticOverflowLeaveInputsUnchanged)
{
    const auto empty = TCheckModifiers::Create({});
    for (const auto& invalid : {
             TCheckModifiers::Create({.ItemBonus = -1}), TCheckModifiers::Create({.ItemPenalty = -1}),
             TCheckModifiers::Create({.StatusBonus = -1}), TCheckModifiers::Create({.StatusPenalty = -1}),
             TCheckModifiers::Create({.CircumstanceBonus = -1}), TCheckModifiers::Create({.CircumstancePenalty = -1}),
             TCheckModifiers::Create({.UntypedPenalty = -1})}) {
        const auto snapshot = ObjectFields(invalid);
        EXPECT_FALSE(invalid.IsValid());
        EXPECT_THROW(invalid.Total(), std::runtime_error);
        EXPECT_THROW(invalid.Merge(empty), std::runtime_error);
        EXPECT_THROW(empty.Merge(invalid), std::runtime_error);
        EXPECT_EQ(ObjectFields(invalid), snapshot);
    }
    const auto maximum = TCheckModifiers::Create({.UntypedPenalty = std::numeric_limits<int>::max()});
    const auto snapshot = ObjectFields(maximum);
    EXPECT_THROW(maximum.Merge(TCheckModifiers::Create({.UntypedPenalty = 1})), std::overflow_error);
    EXPECT_EQ(ObjectFields(maximum), snapshot);
    const auto overflow = TCheckModifiers::Create({.ItemBonus = std::numeric_limits<int>::max(), .StatusBonus = 1});
    EXPECT_THROW(overflow.Total(), std::overflow_error);
}

TEST(StatisticProfilesTest, ClassDcPresenceRankAndCompleteBaselineAreIndependent)
{
    const auto absent = TClassDCProfile::Create({});
    EXPECT_FALSE(absent.Enabled());
    EXPECT_THROW(absent.Base(1, 4), std::runtime_error);
    const auto ranked = TClassDCProfile::Create({
        .Enabled = true, .KeyAttribute = ECharacteristic::Dexterity,
        .Proficiency = TProficiencyValue::Create({.Rank = EProficiencyRank::Trained}),
    });
    EXPECT_EQ(ranked.Base(1, 4), 17);
    EXPECT_EQ(ranked.Base(4, 4), 20);
    for (const int total : {0, -3, 17}) {
        const auto fixed = TClassDCProfile::Create({
            .Enabled = true,
            .Proficiency = TProficiencyValue::Create({.Rank = EProficiencyRank::Legendary}),
            .Baseline = TStatisticBaseline::Create({.IsFixed = true, .Value = total}),
        });
        EXPECT_EQ(fixed.Base(std::numeric_limits<int>::max(), 100), total);
        EXPECT_EQ(fixed.Proficiency().Rank(), EProficiencyRank::Legendary);
    }
}

TEST(StatisticProfilesTest, LoreSubcategoriesRetainIndependentRanks)
{
    const auto warfare = TLoreSkill::Create({
        .Name = "Warfare", .Proficiency = TProficiencyValue::Create({.Rank = EProficiencyRank::Trained}),
    });
    const auto sailing = TLoreSkill::Create({
        .Name = "Sailing", .Proficiency = TProficiencyValue::Create({.Rank = EProficiencyRank::Expert}),
        .Baseline = TStatisticBaseline::Create({.IsFixed = true, .Value = -2}),
    });
    EXPECT_EQ(warfare.Base(1, 0), 3);
    EXPECT_EQ(warfare.Base(4, 2), 8);
    EXPECT_EQ(sailing.Base(1, 0), -2);
    EXPECT_EQ(sailing.Base(20, 10), -2);
    EXPECT_EQ(sailing.Proficiency().Rank(), EProficiencyRank::Expert);
    EXPECT_THROW(TLoreSkill::Create({}).Base(1, 0), std::runtime_error);
}

}

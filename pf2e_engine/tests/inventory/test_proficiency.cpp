#include <pf2e_engine/inventory/proficiency.h>

#include "state_diff.h"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <map>
#include <stdexcept>

namespace {

TEST(ProficiencyTest, RankBonusesUseTheSuppliedLevelOnEveryQuery)
{
    struct TExpectedRank {
        EProficiencyRank Rank;
        int FirstLevel;
        int TwelfthLevel;
    };
    const std::array cases{
        TExpectedRank{EProficiencyRank::Untrained, 0, 0},
        TExpectedRank{EProficiencyRank::Trained, 3, 14},
        TExpectedRank{EProficiencyRank::Expert, 5, 16},
        TExpectedRank{EProficiencyRank::Master, 7, 18},
        TExpectedRank{EProficiencyRank::Legendary, 9, 20},
    };

    for (const auto& expected : cases) {
        const TProficiencyValue value = TProficiencyValue::Create({.Rank = expected.Rank});
        EXPECT_EQ(value.Bonus(1), expected.FirstLevel);
        EXPECT_EQ(value.Bonus(12), expected.TwelfthLevel);
        EXPECT_EQ(value.Bonus(1), expected.FirstLevel);
    }
}

TEST(ProficiencyTest, FixedContributionsOverrideRanksIncludingZeroAndNegativeValues)
{
    const TProficiencyValue negative = TProficiencyValue::Create({
        .Rank = EProficiencyRank::Legendary, .IsFixed = true, .FixedBonus = -4
    });
    const TProficiencyValue zero = TProficiencyValue::Create({
        .Rank = EProficiencyRank::Master, .IsFixed = true, .FixedBonus = 0
    });
    const TProficiencyValue positive = TProficiencyValue::Create({
        .Rank = EProficiencyRank::Untrained, .IsFixed = true, .FixedBonus = 11
    });

    for (int level : {-1, 1, 20, std::numeric_limits<int>::max()}) {
        EXPECT_EQ(negative.Bonus(level), -4);
        EXPECT_EQ(zero.Bonus(level), 0);
        EXPECT_EQ(positive.Bonus(level), 11);
    }
}

TEST(ProficiencyTest, MissingCategoriesAndPerceptionReturnZeroWithoutMutatingMaps)
{
    const TProficiency profile = TProficiency::Create({});
    TAstContext before_context;
    const auto ast = profile.GetAst(before_context);

    EXPECT_EQ(profile.ArmorBonus(EArmorCategory::Heavy, 20), 0);
    EXPECT_EQ(profile.WeaponBonus(EWeaponCategory::Martial, 20), 0);
    EXPECT_EQ(profile.SaveBonus(ESavethrow::Reflex, 20), 0);
    EXPECT_EQ(profile.SkillBonus(ESkill::Thievery, 20), 0);
    EXPECT_EQ(profile.PerceptionBonus(20), 0);
    EXPECT_TRUE(profile.ArmorCategory().empty());
    EXPECT_TRUE(profile.WeaponCategory().empty());
    EXPECT_TRUE(profile.Savethrow().empty());
    EXPECT_TRUE(profile.Skill().empty());
    TAstContext after_context;
    EXPECT_EQ(profile.GetAst(after_context), ast);
}

TEST(ProficiencyTest, IndependentMapsAndPerceptionKeepTheirOwnRules)
{
    const auto ranked = [](EProficiencyRank rank) { return TProficiencyValue::Create({.Rank = rank}); };
    const auto fixed = [](int bonus) { return TProficiencyValue::Create({.IsFixed = true, .FixedBonus = bonus}); };
    const TProficiency profile = TProficiency::Create({
        .ArmorCategory = std::map<EArmorCategory, TProficiencyValue>{{EArmorCategory::Heavy, ranked(EProficiencyRank::Trained)}},
        .WeaponCategory = std::map<EWeaponCategory, TProficiencyValue>{{EWeaponCategory::Martial, ranked(EProficiencyRank::Master)}},
        .Savethrow = std::map<ESavethrow, TProficiencyValue>{
            {ESavethrow::Fortitude, ranked(EProficiencyRank::Expert)}, {ESavethrow::Will, fixed(-1)}},
        .Skill = std::map<ESkill, TProficiencyValue>{{ESkill::Thievery, ranked(EProficiencyRank::Legendary)}},
        .Perception = fixed(11),
    });

    EXPECT_EQ(profile.ArmorBonus(EArmorCategory::Heavy, 3), 5);
    EXPECT_EQ(profile.ArmorBonus(EArmorCategory::Light, 3), 0);
    EXPECT_EQ(profile.WeaponBonus(EWeaponCategory::Martial, 3), 9);
    EXPECT_EQ(profile.WeaponBonus(EWeaponCategory::Simple, 3), 0);
    EXPECT_EQ(profile.SaveBonus(ESavethrow::Fortitude, 3), 7);
    EXPECT_EQ(profile.SaveBonus(ESavethrow::Will, 3), -1);
    EXPECT_EQ(profile.SaveBonus(ESavethrow::Reflex, 3), 0);
    EXPECT_EQ(profile.SkillBonus(ESkill::Thievery, 3), 11);
    EXPECT_EQ(profile.SkillBonus(ESkill::Athletics, 3), 0);
    EXPECT_EQ(profile.PerceptionBonus(3), 11);
}

TEST(ProficiencyTest, RankArithmeticChecksOverflowAndQueriesLeaveStateIntact)
{
    const TProficiencyValue value = TProficiencyValue::Create({.Rank = EProficiencyRank::Trained});
    const auto before = ObjectFields(value);

    EXPECT_EQ(value.Bonus(-3), -1);
    EXPECT_THROW(value.Bonus(std::numeric_limits<int>::max()), std::overflow_error);
    EXPECT_EQ(ObjectFields(value), before);
    EXPECT_EQ(value.Bonus(1), 3);
}

}

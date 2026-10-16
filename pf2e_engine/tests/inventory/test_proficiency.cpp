#include <pf2e_engine/inventory/proficiency.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <array>
#include <limits>
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
    TGameObjectFactory factory;
    const TProficiency profile = TProficiency::FromJson(nlohmann::json::object(), factory);
    const auto before = profile.ToJson();
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
    EXPECT_EQ(profile.ToJson(), before);
    TAstContext after_context;
    EXPECT_EQ(profile.GetAst(after_context), ast);
}

TEST(ProficiencyTest, IndependentMapsAndPerceptionKeepTheirOwnRules)
{
    TGameObjectFactory factory;
    const TProficiency profile = TProficiency::FromJson({
        {"armor_category", {{"Heavy", {{"rank", "Trained"}}}}},
        {"weapon_category", {{"Martial", {{"rank", "Master"}}}}},
        {"savethrow", {
            {"Fortitude", {{"rank", "Expert"}}},
            {"Will", {{"is_fixed", true}, {"fixed_bonus", -1}}}
        }},
        {"skill", {{"Thievery", {{"rank", "Legendary"}}}}},
        {"perception", {{"is_fixed", true}, {"fixed_bonus", 11}}}
    }, factory);

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

TEST(ProficiencyTest, CanonicalEnumKeysAndValuesSurviveSnapshotRestore)
{
    TGameObjectFactory factory;
    const TProficiency profile = TProficiency::FromJson({
        {"armor_category", {{"Unarmored", {{"rank", "Trained"}}}}},
        {"weapon_category", {{"Unarmed", {{"rank", "Expert"}}}}},
        {"savethrow", {{"Will", {{"rank", "Master"}}}}},
        {"skill", {{"Athletics", {{"rank", "Legendary"}}}}},
        {"perception", {{"is_fixed", true}, {"fixed_bonus", 0}}}
    }, factory);
    const auto snapshot = profile.ToJson();
    const TProficiency restored = TProficiency::Restore(snapshot, factory);

    EXPECT_EQ(snapshot["armor_category"]["Unarmored"]["rank"], "Trained");
    EXPECT_EQ(snapshot["weapon_category"]["Unarmed"]["rank"], "Expert");
    EXPECT_EQ(snapshot["savethrow"]["Will"]["rank"], "Master");
    EXPECT_EQ(snapshot["skill"]["Athletics"]["rank"], "Legendary");
    EXPECT_FALSE(snapshot.contains("level"));
    EXPECT_EQ(restored.ToJson(), snapshot);
    EXPECT_EQ(restored.ArmorBonus(EArmorCategory::Unarmored, 5), 7);
    EXPECT_EQ(restored.WeaponBonus(EWeaponCategory::Unarmed, 5), 9);
    EXPECT_EQ(restored.SaveBonus(ESavethrow::Will, 5), 11);
    EXPECT_EQ(restored.SkillBonus(ESkill::Athletics, 5), 13);
    EXPECT_EQ(restored.PerceptionBonus(5), 0);
}

TEST(ProficiencyTest, InvalidEnumInputsAreRejected)
{
    TGameObjectFactory factory;
    EXPECT_THROW(TProficiencyValue::FromJson({{"rank", "Unexpected"}}, factory),
                 std::runtime_error);
    EXPECT_THROW(TProficiency::FromJson({
        {"skill", {{"COUNT", {{"rank", "Trained"}}}}}
    }, factory), std::runtime_error);
    EXPECT_THROW(TProficiency::FromJson({
        {"savethrow", {{"FortitudeTypo", {{"rank", "Trained"}}}}}
    }, factory), std::runtime_error);
}

TEST(ProficiencyTest, RankArithmeticChecksOverflowAndQueriesLeaveStateIntact)
{
    const TProficiencyValue value = TProficiencyValue::Create({.Rank = EProficiencyRank::Trained});
    const auto before = value.ToJson();

    EXPECT_EQ(value.Bonus(-3), -1);
    EXPECT_THROW(value.Bonus(std::numeric_limits<int>::max()), std::overflow_error);
    EXPECT_EQ(value.ToJson(), before);
    EXPECT_EQ(value.Bonus(1), 3);
}

}

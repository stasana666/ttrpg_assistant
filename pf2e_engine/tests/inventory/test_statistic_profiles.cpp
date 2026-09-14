#include <pf2e_engine/creature.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/inventory/check_modifiers.h>
#include <pf2e_engine/inventory/lore.h>
#include <pf2e_engine/inventory/statistics.h>
#include <pf2e_engine/mechanics/savethrows.h>
#include <pf2e_engine/mechanics/skills.h>

#include <cpp_config.h>
#include <gtest/gtest.h>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void ValidateProfile(const std::string& definition, const nlohmann::json& value)
{
    std::ifstream input(kRootDirPath + "/pf2e_engine/schemas/schema.json");
    const auto source = nlohmann::json::parse(input);
    const nlohmann::json schema = {
        {"$ref", "#/$defs/" + definition}, {"$defs", source.at("$defs")},
    };
    nlohmann::json_schema::json_validator validator(schema);
    validator.validate(value);
}

TEST(StatisticProfilesTest, SkillAndSaveAttributesAreGeneratedAndLegacyAdaptersAgree)
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
        EXPECT_EQ(BindedCharacteristic(skill), attribute);
    }
    for (const auto& [save, attribute] : std::array{
             std::pair{ESavethrow::Fortitude, ECharacteristic::Constitution},
             std::pair{ESavethrow::Reflex, ECharacteristic::Dexterity},
             std::pair{ESavethrow::Will, ECharacteristic::Wisdom}}) {
        EXPECT_EQ(rules.SaveAttribute(save), attribute);
        EXPECT_EQ(BindedCharacteristic(save), attribute);
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
    const auto first_json = first.ToJson();
    const auto second_json = second.ToJson();
    const auto merged = first.Merge(second);
    EXPECT_EQ(merged.ItemBonus(), 3);
    EXPECT_EQ(merged.ItemPenalty(), 2);
    EXPECT_EQ(merged.StatusBonus(), 3);
    EXPECT_EQ(merged.StatusPenalty(), 4);
    EXPECT_EQ(merged.CircumstanceBonus(), 2);
    EXPECT_EQ(merged.CircumstancePenalty(), 3);
    EXPECT_EQ(merged.UntypedPenalty(), 3);
    EXPECT_EQ(merged.Total(), -4);
    EXPECT_EQ(merged.ToJson(), second.Merge(first).ToJson());
    EXPECT_EQ(first.ToJson(), first_json);
    EXPECT_EQ(second.ToJson(), second_json);
    EXPECT_EQ(first.Merge(TCheckModifiers::Create({})).ToJson(), first_json);
}

TEST(StatisticProfilesTest, ModifierMergingIsAssociativeAndUntypedPenaltiesAccumulate)
{
    const auto first = TCheckModifiers::Create({.StatusBonus = 2, .UntypedPenalty = 1});
    const auto second = TCheckModifiers::Create({.StatusBonus = 2, .UntypedPenalty = 3});
    const auto third = TCheckModifiers::Create({.StatusPenalty = 1, .CircumstanceBonus = 4});
    const auto result = first.Merge(second).Merge(third);
    EXPECT_EQ(result.ToJson(), first.Merge(second.Merge(third)).ToJson());
    EXPECT_EQ(result.Total(), 1);
    EXPECT_EQ(first.Merge(first).StatusBonus(), 2);
    EXPECT_EQ(first.Merge(first).UntypedPenalty(), 2);
}

TEST(StatisticProfilesTest, InvalidModifiersAndArithmeticOverflowLeaveInputsUnchanged)
{
    TGameObjectFactory factory;
    const auto empty = TCheckModifiers::Create({});
    for (const auto* field : {"item_bonus", "item_penalty", "status_bonus", "status_penalty",
                              "circumstance_bonus", "circumstance_penalty", "untyped_penalty"}) {
        const auto invalid = TCheckModifiers::FromJson({{field, -1}}, factory);
        const auto snapshot = invalid.ToJson();
        EXPECT_FALSE(invalid.IsValid());
        EXPECT_THROW(invalid.Total(), std::runtime_error);
        EXPECT_THROW(invalid.Merge(empty), std::runtime_error);
        EXPECT_THROW(empty.Merge(invalid), std::runtime_error);
        EXPECT_EQ(invalid.ToJson(), snapshot);
    }
    const auto maximum = TCheckModifiers::Create({.UntypedPenalty = std::numeric_limits<int>::max()});
    const auto snapshot = maximum.ToJson();
    EXPECT_THROW(maximum.Merge(TCheckModifiers::Create({.UntypedPenalty = 1})), std::overflow_error);
    EXPECT_EQ(maximum.ToJson(), snapshot);
    const auto overflow = TCheckModifiers::Create({.ItemBonus = std::numeric_limits<int>::max(), .StatusBonus = 1});
    EXPECT_THROW(overflow.Total(), std::overflow_error);
    for (const auto& invalid : {nlohmann::json(0.5), nlohmann::json(true),
                               nlohmann::json(std::numeric_limits<std::uint64_t>::max())}) {
        EXPECT_THROW(TCheckModifiers::FromJson({{"status_bonus", invalid}}, factory), std::exception);
    }
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

TEST(StatisticProfilesTest, LoreSubcategoriesRetainIndependentRanksAndSnapshots)
{
    TGameObjectFactory factory;
    const auto warfare = TLoreSkill::FromJson({
        {"name", "Warfare"}, {"proficiency", {{"rank", "Trained"}}},
    }, factory);
    const auto sailing = TLoreSkill::FromJson({
        {"name", "Sailing"}, {"proficiency", {{"rank", "Expert"}}},
        {"baseline", {{"is_fixed", true}, {"value", -2}}},
    }, factory);
    EXPECT_EQ(warfare.Base(1, 0), 3);
    EXPECT_EQ(warfare.Base(4, 2), 8);
    EXPECT_EQ(sailing.Base(1, 0), -2);
    EXPECT_EQ(sailing.Base(20, 10), -2);
    EXPECT_EQ(sailing.Proficiency().Rank(), EProficiencyRank::Expert);
    const auto restored = TLoreSkill::Restore(sailing.ToJson(), factory);
    EXPECT_EQ(restored.ToJson(), sailing.ToJson());
    EXPECT_EQ(restored.Name(), "Sailing");
    EXPECT_FALSE(restored.ToJson().contains("level"));
    EXPECT_THROW(TLoreSkill::Create({}).Base(1, 0), std::runtime_error);
}

TEST(StatisticProfilesTest, CanonicalCreatureProfilesRestoreWithoutRecomputingTotals)
{
    TGameObjectFactory factory;
    const auto creature = TCreature::FromJson({
        {"level", 1}, {"hitpoints", {{"maximum", {{"base", 1}}}}},
        {"perception_baseline", {{"is_fixed", true}, {"value", 0}}},
        {"save_baselines", {{"Fortitude", {{"is_fixed", true}, {"value", -2}}}}},
        {"skill_baselines", {{"Athletics", {{"is_fixed", true}, {"value", 6}}}}},
        {"proficiency", {{"skill", {{"Athletics", {{"rank", "Trained"}}}}}}},
        {"class_dc_profile", {{"enabled", true}, {"baseline", {{"is_fixed", true}, {"value", 17}}}}},
        {"lore_skills", nlohmann::json::array({{{"name", "Warfare"}, {"proficiency", {{"rank", "Trained"}}}}})},
    }, factory);
    const auto snapshot = creature.ToJson();
    EXPECT_TRUE(snapshot.at("class_dc_profile").at("enabled").get<bool>());
    EXPECT_FALSE(snapshot.contains("class_d_c_profile"));
    const auto restored = TCreature::Restore(snapshot, factory);
    EXPECT_EQ(restored.ToJson(), snapshot);
    EXPECT_EQ(restored.PerceptionBonus(), 0);
    EXPECT_EQ(restored.SaveBonus(ESavethrow::Fortitude), -2);
    EXPECT_EQ(restored.SkillBonus(ESkill::Athletics), 6);
    EXPECT_EQ(restored.SkillProficiency(ESkill::Athletics), 3);
    EXPECT_EQ(restored.ClassDC(), 17);
    ASSERT_EQ(restored.LoreSkills().Size(), 1u);
    EXPECT_EQ((**restored.LoreSkills().begin()).Name(), "Warfare");
    TAstContext first_context;
    TAstContext second_context;
    EXPECT_EQ(creature.GetAst(first_context), restored.GetAst(second_context));
}

TEST(StatisticProfilesTest, JsonSchemaAcceptsProfilesAndRejectsInvalidKeysAndValues)
{
    EXPECT_NO_THROW(ValidateProfile("pf2e_save_baselines", {{"Reflex", {{"is_fixed", true}, {"value", 0}}}}));
    EXPECT_NO_THROW(ValidateProfile("pf2e_skill_baselines", {{"Athletics", {{"is_fixed", true}, {"value", -4}}}}));
    EXPECT_NO_THROW(ValidateProfile("pf2e_class_dc_profile", {
        {"enabled", true}, {"key_attribute", "Strength"}, {"proficiency", {{"rank", "Trained"}}},
    }));
    EXPECT_NO_THROW(ValidateProfile("pf2e_lore_skill", {{"name", "Warfare"}}));
    EXPECT_THROW(ValidateProfile("pf2e_save_baselines", {{"Misspelled", {{"value", 4}}}}), std::exception);
    EXPECT_THROW(ValidateProfile("pf2e_skill_baselines", {{"COUNT", {{"value", 4}}}}), std::exception);
    EXPECT_THROW(ValidateProfile("pf2e_class_dc_profile", {{"key_attribute", "Invalid"}}), std::exception);
    EXPECT_THROW(ValidateProfile("pf2e_lore_skill", {{"name", ""}}), std::exception);
    for (const auto& invalid : {nlohmann::json(1.5), nlohmann::json(true),
                               nlohmann::json(std::numeric_limits<std::int64_t>::max())}) {
        EXPECT_THROW(ValidateProfile("pf2e_save_baselines", {{"Reflex", {{"value", invalid}}}}), std::exception);
        EXPECT_THROW(ValidateProfile("pf2e_class_dc_profile", {{"baseline", {{"value", invalid}}}}), std::exception);
    }
}

}

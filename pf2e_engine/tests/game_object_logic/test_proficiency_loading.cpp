#include <pf2e_engine/creature.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <cpp_config.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>

namespace {

const std::filesystem::path kProficiencyData{kRootDirPath + "/pf2e_engine/data"};

class TProficiencyLoadingTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        directory_ = std::filesystem::temp_directory_path() /
            ("ttrpg-proficiency-loading-" + std::to_string(stamp));
        ASSERT_TRUE(std::filesystem::create_directory(directory_));
        for (const auto* source : {"inventory/material/leather.json", "inventory/armor/unarmored.json",
                "inventory/weapon/longsword.json", "creatures/races/human.json", "creatures/classes/fighter.json"}) {
            factory_.AddSource(kProficiencyData / source);
        }
    }

    void TearDown() override { std::filesystem::remove_all(directory_); }

    nlohmann::json Definition() const {
        std::ifstream input(kProficiencyData / "creatures/warrior.json");
        auto result = nlohmann::json::parse(input);
        result["pf2e_creature"].erase("proficiency");
        result["pf2e_creature"]["creature_data"]["features"] = nlohmann::json::array();
        return result;
    }

    std::filesystem::path Write(nlohmann::json definition) {
        const std::string name = "proficiency_fixture_" + std::to_string(next_id_++);
        definition["name"] = name;
        const auto path = directory_ / (name + ".json");
        std::ofstream output(path);
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output << definition;
        output.close();
        return path;
    }

    TCreature Load(nlohmann::json definition) {
        const auto path = Write(std::move(definition));
        factory_.AddSource(path);
        return factory_.Create<TCreature>(
            TGameObjectIdManager::Instance().Register(path.stem().string()));
    }

    TGameObjectFactory factory_;
    std::filesystem::path directory_;
    int next_id_ = 0;
};

TEST_F(TProficiencyLoadingTest, LegacyRanksAndFixedContributionsBecomeCanonicalData)
{
    auto definition = Definition();
    definition["pf2e_creature"]["proficiency"] = {
        {"skill", {{"Athletics", "master"}}},
        {"weapon_category", {{"Martial", 7}}}, {"Perception", -3}
    };
    const TCreature creature = Load(std::move(definition));
    const auto canonical = creature.Proficiency().ToJson();
    EXPECT_EQ(canonical["skill"]["Athletics"]["rank"], "Master");
    EXPECT_EQ(canonical["weapon_category"]["Martial"]["is_fixed"], true);
    EXPECT_EQ(canonical["weapon_category"]["Martial"]["fixed_bonus"], 7);
    EXPECT_EQ(canonical["perception"]["is_fixed"], true);
    EXPECT_EQ(canonical["perception"]["fixed_bonus"], -3);
    EXPECT_FALSE(canonical.contains("Perception"));
    EXPECT_FALSE(canonical.contains("level"));
}

TEST_F(TProficiencyLoadingTest, NestedCanonicalProfileLoadsWithoutLegacyEnvelope)
{
    auto definition = Definition();
    definition["pf2e_creature"]["creature_data"]["proficiency"] = {
        {"armor_category", {{"Unarmored", {{"rank", "Trained"}}}}},
        {"skill", {{"Acrobatics", {{"is_fixed", true}, {"fixed_bonus", -2}}}}},
        {"perception", {{"rank", "Expert"}}}
    };
    const TCreature creature = Load(std::move(definition));
    EXPECT_EQ(creature.Proficiency().ArmorCategory().at(EArmorCategory::Unarmored).Rank(),
              EProficiencyRank::Trained);
    EXPECT_EQ(creature.Proficiency().Skill().at(ESkill::Acrobatics).FixedBonus(), -2);
    EXPECT_EQ(creature.Proficiency().Perception().Rank(), EProficiencyRank::Expert);
}

TEST_F(TProficiencyLoadingTest, EmptyAndAbsentLegacyProfilesDefaultToUntrained)
{
    for (bool include_empty : {false, true}) {
        auto definition = Definition();
        if (include_empty) {
            definition["pf2e_creature"]["proficiency"] = nlohmann::json::object();
        }
        const TCreature creature = Load(std::move(definition));
        EXPECT_TRUE(creature.Proficiency().ArmorCategory().empty());
        EXPECT_TRUE(creature.Proficiency().WeaponCategory().empty());
        EXPECT_TRUE(creature.Proficiency().Savethrow().empty());
        EXPECT_TRUE(creature.Proficiency().Skill().empty());
        EXPECT_EQ(creature.PerceptionProficiency(), 0);
    }
}

TEST_F(TProficiencyLoadingTest, ConflictingLocationsAreRejectedBeforeDefinitionIsPublished)
{
    auto definition = Definition();
    definition["pf2e_creature"]["proficiency"] = nlohmann::json::object();
    definition["pf2e_creature"]["creature_data"]["proficiency"] = nlohmann::json::object();
    const auto path = Write(std::move(definition));
    EXPECT_THROW(factory_.AddSource(path), std::exception);
    EXPECT_TRUE(factory_.AllKnown<TCreature>().empty());
}

TEST_F(TProficiencyLoadingTest, LegacyInvalidRanksKeysAndIncompatibleValuesAreRejected)
{
    for (const nlohmann::json& invalid : {nlohmann::json("invented_rank"), nlohmann::json(true),
            nlohmann::json(1.5), nlohmann::json{{"rank", "Trained"}},
            nlohmann::json(std::numeric_limits<std::int64_t>::max()),
            nlohmann::json(std::numeric_limits<std::int64_t>::min()),
            nlohmann::json(std::numeric_limits<std::uint64_t>::max())}) {
        auto definition = Definition();
        definition["pf2e_creature"]["proficiency"] = {{"skill", {{"Athletics", invalid}}}};
        EXPECT_THROW(Load(std::move(definition)), std::exception) << invalid.dump();
    }
    auto definition = Definition();
    definition["pf2e_creature"]["proficiency"] = {{"skill", {{"MisspelledSkill", "trained"}}}};
    EXPECT_THROW(Load(std::move(definition)), std::exception);
}

TEST_F(TProficiencyLoadingTest, CanonicalFixedBonusIsValidatedBeforeIntegerConversion)
{
    for (const nlohmann::json& invalid : {nlohmann::json(1.5), nlohmann::json("4"),
            nlohmann::json(std::numeric_limits<std::int64_t>::max()),
            nlohmann::json(std::numeric_limits<std::int64_t>::min())}) {
        auto definition = Definition();
        definition["pf2e_creature"]["creature_data"]["proficiency"] = {
            {"perception", {{"is_fixed", true}, {"fixed_bonus", invalid}}}
        };
        EXPECT_THROW(Load(std::move(definition)), std::exception) << invalid.dump();
    }
}

}

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/inventory/creature_description.h>
#include <pf2e_engine/proficiency.h>
#include <pf2e_engine/transformation/transformator.h>

#include "../test_lib/mock_interaction_system.h"

#include <cpp_config.h>
#include <gtest/gtest.h>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void ValidateDescription(const std::string& definition, const nlohmann::json& value)
{
    std::ifstream input(kRootDirPath + "/pf2e_engine/schemas/schema.json");
    const auto source = nlohmann::json::parse(input);
    const nlohmann::json schema = {
        {"$ref", "#/$defs/" + definition}, {"$defs", source.at("$defs")},
    };
    nlohmann::json_schema::json_validator validator(schema);
    validator.validate(value);
}

TAstNode Snapshot(const TCreature& creature, const TTransformator& journal)
{
    TAstContext context;
    context.RegisterIdentity(&creature, "described_creature");
    auto node = TAstNode::MakeObject("creature_description");
    AddOwnedObject(node, "creature", creature, context);
    AddOwnedObject(node, "journal", journal, context);
    return node;
}

TEST(CreatureDescriptionTest, NamesTraitsLanguagesAndSensePrecisionRoundTrip)
{
    TGameObjectFactory factory;
    const auto default_sense = TSense::FromJson(nlohmann::json::object(), factory);
    EXPECT_EQ(default_sense.Kind(), ESenseKind::Vision);
    EXPECT_EQ(default_sense.Name(), "");
    EXPECT_EQ(default_sense.Precision(), ESensePrecision::Precise);
    EXPECT_EQ(default_sense.RangeFeet(), 0);
    const auto creature = TCreature::FromJson({
        {"name", "Test scout"}, {"traits", {"Human", "Humanoid"}},
        {"languages", nlohmann::json::array({{{"name", "Common"}}, {{"name", "Local Sign"}}})},
        {"senses", nlohmann::json::array({
            {{"kind", "LowLightVision"}, {"name", "low-light vision"}},
            {{"kind", "Scent"}, {"name", "scent"}, {"range_feet", 30}, {"precision", "Imprecise"}},
        })},
        {"level", 1}, {"race", {{"name", "Human"}, {"hitpoints", 8}}},
        {"class", {{"name", "Fighter"}, {"hitpoints", 10}}},
    }, factory);
    EXPECT_EQ(creature.Name(), "Test scout");
    EXPECT_EQ(creature.Race().Name(), "Human");
    EXPECT_EQ(creature.Class().Name(), "Fighter");
    EXPECT_EQ(creature.Traits(), (std::set<ECreatureTrait>{ECreatureTrait::Human, ECreatureTrait::Humanoid}));
    ASSERT_EQ(creature.Languages().Size(), 2u);
    EXPECT_EQ(creature.Languages().Find(TId<TLanguage>{1})->Name(), "Local Sign");
    ASSERT_EQ(creature.Senses().Size(), 2u);
    const auto& vision = *creature.Senses().Find(TId<TSense>{0});
    const auto& scent = *creature.Senses().Find(TId<TSense>{1});
    EXPECT_EQ(vision.Kind(), ESenseKind::LowLightVision);
    EXPECT_EQ(vision.Precision(), ESensePrecision::Precise);
    EXPECT_EQ(vision.RangeFeet(), 0);
    EXPECT_EQ(scent.Kind(), ESenseKind::Scent);
    EXPECT_EQ(scent.Precision(), ESensePrecision::Imprecise);
    EXPECT_EQ(scent.RangeFeet(), 30);
    const auto json = creature.ToJson();
    const auto restored = TCreature::Restore(json, factory);
    EXPECT_EQ(restored.ToJson(), json);
    TAstContext first_context;
    TAstContext second_context;
    EXPECT_EQ(creature.GetAst(first_context), restored.GetAst(second_context));
}

TEST(CreatureDescriptionTest, CollectionsPreserveHolesAndDescriptionChangesUndoExactly)
{
    TIdCollection<TLanguage> languages;
    languages.Add(TLanguage::Create({.Name = "Common"}));
    const auto removed = languages.Add(TLanguage::Create({.Name = "Old dialect"}));
    languages.Remove(removed);
    const auto dialect = languages.Add(TLanguage::Create({.Name = "River dialect"}));
    auto creature = TCreature::Create({.Name = "Speaker", .Languages = std::move(languages)});
    TGameObjectFactory factory;
    const auto json = creature.ToJson();
    const auto restored = TCreature::Restore(json, factory);
    EXPECT_EQ(restored.Languages().NextId().Value, 3u);
    EXPECT_EQ(restored.Languages().Find(removed), nullptr);
    ASSERT_NE(restored.Languages().Find(dialect), nullptr);
    EXPECT_EQ(restored.Languages().Find(dialect)->Name(), "River dialect");
    EXPECT_EQ(restored.ToJson(), json);

    TMockInteractionSystem io;
    TTransformator journal(io);
    auto execution = journal.ExecutionContext();
    const auto checkpoint = journal.CurrentState();
    const auto before = Snapshot(creature, journal);
    execution.AssignGuarded(creature.Name(), std::string("Renamed"),
                            TGuarded<TCreature>(creature), "name");
    execution.AssignGuarded(creature.Languages(), TIdCollection<TLanguage>{},
                            TGuarded<TCreature>(creature), "languages");
    EXPECT_EQ(creature.Languages()->Size(), 0u);
    EXPECT_NE(Snapshot(creature, journal), before);
    journal.Undo(checkpoint);
    EXPECT_EQ(Snapshot(creature, journal), before);
    EXPECT_EQ(creature.ToJson(), json);
}

TEST(CreatureDescriptionTest, AdvancedWeaponUsesItsOwnTrainedRankAndCurrentLevel)
{
    TGameObjectFactory factory;
    const auto weapon = TWeapon::FromJson({
        {"name", "Declared advanced weapon"}, {"base_dice_size", 8},
        {"damage_type", "Slashing"}, {"category", "Advanced"},
    }, factory);
    auto creature = TCreature::Create({
        .Level = 1,
        .Characteristic = TAbilityScores::Create({.Strength = TAbilityScore::Create({.Value = 18})}),
        .Proficiency = TProficiency::Create({.WeaponCategory = std::map<EWeaponCategory, TProficiencyValue>{
            {EWeaponCategory::Martial, TProficiencyValue::Create({.Rank = EProficiencyRank::Expert})},
            {EWeaponCategory::Advanced, TProficiencyValue::Create({.Rank = EProficiencyRank::Trained})},
        }}),
    });
    EXPECT_EQ(creature.WeaponProficiency(EWeaponCategory::Advanced), 3);
    EXPECT_EQ(ProficiencyBonus(creature, weapon), 3);
    EXPECT_EQ(creature.AttackBonus(weapon), 7);
    EXPECT_EQ(creature.WeaponProficiency(EWeaponCategory::Martial), 5);
    EXPECT_EQ(weapon.ToJson().at("category"), "Advanced");
    EXPECT_EQ(TWeapon::Restore(weapon.ToJson(), factory).Category(), EWeaponCategory::Advanced);
    TMockInteractionSystem io;
    TTransformator journal(io);
    const auto checkpoint = journal.CurrentState();
    const auto before = Snapshot(creature, journal);
    auto execution = journal.ExecutionContext();
    execution.AssignGuarded(creature.Level(), 4, TGuarded<TCreature>(creature), "level");
    EXPECT_EQ(creature.AttackBonus(weapon), 10);
    journal.Undo(checkpoint);
    EXPECT_EQ(Snapshot(creature, journal), before);
    EXPECT_EQ(creature.AttackBonus(weapon), 7);
}

TEST(CreatureDescriptionTest, DisplayFeetDeriveFromExistingCellsWithoutDuplicateStorage)
{
    auto creature = TCreature::Create({.Movement = 5});
    const auto weapon = TWeapon::Create({.Reach = 2});
    const auto before = creature.ToJson();
    EXPECT_EQ(creature.SpeedFeet(), 25);
    EXPECT_EQ(weapon.ReachFeet(), 10);
    EXPECT_FALSE(before.contains("speed_feet"));
    EXPECT_FALSE(weapon.ToJson().contains("reach_feet"));
    EXPECT_EQ(creature.ToJson(), before);
    TMockInteractionSystem io;
    TTransformator journal(io);
    const auto checkpoint = journal.CurrentState();
    auto execution = journal.ExecutionContext();
    execution.AssignGuarded(creature.Movement(), 7, TGuarded<TCreature>(creature), "movement");
    EXPECT_EQ(creature.SpeedFeet(), 35);
    journal.Undo(checkpoint);
    EXPECT_EQ(creature.ToJson(), before);
    const auto excessive_speed = TCreature::Create({.Movement = std::numeric_limits<int>::max()});
    const auto excessive_reach = TWeapon::Create({.Reach = std::numeric_limits<int>::max()});
    EXPECT_THROW(excessive_speed.SpeedFeet(), std::overflow_error);
    EXPECT_THROW(excessive_reach.ReachFeet(), std::overflow_error);
}

TEST(CreatureDescriptionTest, JsonSchemaAcceptsMetadataAndAdvancedInBothProficiencyShapes)
{
    const nlohmann::json profile = {
        {"level", 1}, {"size", "Medium"}, {"race", "human"}, {"class", "fighter"},
        {"characteristic", "scores"}, {"armor", "unarmored"},
        {"name", "Described creature"}, {"traits", {"Animal"}},
        {"languages", nlohmann::json::array({{{"name", "Custom language"}}})},
        {"senses", nlohmann::json::array({
            {{"kind", "Scent"}, {"range_feet", 30}, {"precision", "Imprecise"}},
        })},
    };
    EXPECT_NO_THROW(ValidateDescription("pf2e_creature_data_core", profile));
    EXPECT_NO_THROW(ValidateDescription("pf2e_race_core", {{"name", "Human"}, {"hitpoints", 8}}));
    EXPECT_NO_THROW(ValidateDescription("pf2e_class_core", {{"name", "Fighter"}, {"hitpoints", 10}}));
    EXPECT_NO_THROW(ValidateDescription("pf2e_weapon_core", {
        {"name", "Declared"}, {"base_dice_size", 8}, {"damage_type", "Slashing"}, {"category", "Advanced"},
    }));
    EXPECT_NO_THROW(ValidateDescription("pf2e_proficiency", {{"weapon_category", {{"Advanced", "trained"}}}}));
    EXPECT_NO_THROW(ValidateDescription("pf2e_proficiency_data", {
        {"weapon_category", {{"Advanced", {{"rank", "Trained"}}}}},
    }));
    auto invalid = profile;
    invalid["traits"] = {"UnknownTrait"};
    EXPECT_THROW(ValidateDescription("pf2e_creature_data_core", invalid), std::exception);
    EXPECT_THROW(ValidateDescription("pf2e_language", {{"name", ""}}), std::exception);
    EXPECT_THROW(ValidateDescription("pf2e_sense", {{"kind", "UnknownSense"}}), std::exception);
    EXPECT_THROW(ValidateDescription("pf2e_sense", {{"range_feet", -1}}), std::exception);
    EXPECT_THROW(ValidateDescription("pf2e_sense", {{"precision", "UnknownPrecision"}}), std::exception);
}

}

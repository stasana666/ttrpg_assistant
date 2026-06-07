#include <game_object_factory.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>

#include <cpp_config.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <sstream>

#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/material.h>
#include "battle_map.h"
#include "characteristics.h"
#include "game_object_id.h"
#include "proficiency.h"
#include <pf2e_engine/inventory/weapon.h>

#include <pf2e_engine/actions/action_reader.h>
#include <pf2e_engine/feat.h>

const std::string kPathToSchema = kRootDirPath + "/pf2e_engine/schemas/schema.json";

const std::unordered_map<std::string, TGameObjectFactory::FMethod>
TGameObjectFactory::kReaderMapping = {
    {"pf2e_armor", &TGameObjectFactory::ReadArmor},
    {"pf2e_material", &TGameObjectFactory::ReadMaterial},
    {"pf2e_weapon", &TGameObjectFactory::ReadWeapon},
    {"pf2e_race", &TGameObjectFactory::ReadRace},
    {"pf2e_class", &TGameObjectFactory::ReadClass},
    {"pf2e_ability_score", &TGameObjectFactory::ReadAbilityScore},
    {"pf2e_ability_scores", &TGameObjectFactory::ReadAbilityScores},
    {"pf2e_creature_data", &TGameObjectFactory::ReadCreatureData},
    {"pf2e_creature", &TGameObjectFactory::ReadCreature},
    {"pf2e_action", &TGameObjectFactory::ReadAction},
    {"pf2e_battle_map", &TGameObjectFactory::ReadBattleMap},
};

void TGameObjectFactory::AddSource(const std::filesystem::path& source_path)
{
    if (!std::filesystem::exists(source_path)) {
        std::stringstream ss;
        ss << "File by path \"" << source_path << "\" not found";
        throw std::runtime_error(ss.str());
    }
    if (std::filesystem::is_regular_file(source_path)) {
        return ReadObjectFromFile(source_path);
    }
    if (std::filesystem::is_directory(source_path)) {
        for (auto de : std::filesystem::directory_iterator{source_path}) {
            if (de.is_regular_file()) {
                ReadObjectFromFile(de);
            }
        }
        return;
    }
    throw std::runtime_error("not supported source type: \"" + source_path.string() + "\"");
}

void TGameObjectFactory::ReadObjectFromFile(const std::filesystem::path& game_object_file)
{
    std::ifstream game_object_stream(game_object_file);
    nlohmann::json json_game_object = nlohmann::json::parse(game_object_stream);
    ValidateObject(json_game_object);
    
    auto reader = kReaderMapping.find(json_game_object["type"]);

    if (reader == kReaderMapping.end()) {
        std::stringstream ss;
        ss << "unknown type of game object " << json_game_object["type"];
        throw std::runtime_error(ss.str());
    }

    TGameObjectId id = ReadGameObjectName(json_game_object);
    (this->*reader->second)(json_game_object[json_game_object["type"]], id);
}

void TGameObjectFactory::ValidateObject(nlohmann::json& json) const
{
    using Validator = nlohmann::json_schema::json_validator;
    static std::unordered_map<std::string, Validator> validators;
    static nlohmann::json schema = []() {
        std::ifstream in(kPathToSchema);
        nlohmann::json schema = nlohmann::json::parse(in);
        return schema;
    }();
    static Validator object_validator{schema};

    object_validator.validate(json);

    Validator& validator = [&]() -> Validator& {
        auto type = json["type"].get<std::string>();
        if (!validators.contains(type)) {
            schema["$ref"] = "#/$defs/" + type;
            validators.emplace(type, schema);
        }
        return validators.find(type)->second;
    }();

    const auto default_patch = validator.validate(json);
	json = json.patch(default_patch);
}

TGameObjectId TGameObjectFactory::ReadGameObjectName(nlohmann::json& json_game_object) const
{
    std::string name = json_game_object["name"];
    return TGameObjectIdManager::Instance().Register(name);
}


void TGameObjectFactory::ReadArmor(nlohmann::json& json_game_object, TGameObjectId id)
{
    armors_.insert({id, [this, json_game_object]() {
        return TArmor::FromJson(json_game_object, *this);
    }});
}

void TGameObjectFactory::ReadMaterial(nlohmann::json& json_game_object, TGameObjectId id)
{
    TMaterial result = TMaterial::FromJson(json_game_object, *this);
    materials_.insert({id, [result]() { return result; }});
}


void TGameObjectFactory::ReadWeapon(nlohmann::json& json_game_object, TGameObjectId id)
{
    TWeapon result = TWeapon::FromJson(json_game_object, *this);
    weapons_.insert({id, [result]() { return result; }});
}


void TGameObjectFactory::ReadRace(nlohmann::json& json_game_object, TGameObjectId id)
{
    TRace result = TRace::FromJson(json_game_object, *this);
    races_.insert({id, [result]() { return result; }});
}

void TGameObjectFactory::ReadClass(nlohmann::json& json_game_object, TGameObjectId id)
{
    TClass result = TClass::FromJson(json_game_object, *this);
    classes_.insert({id, [result]() { return result; }});
}

void TGameObjectFactory::ReadAbilityScore(nlohmann::json& json_game_object, TGameObjectId id)
{
    TAbilityScore result = TAbilityScore::FromJson(json_game_object, *this);
    ability_score_.insert({id, [result]() { return result; }});
}

void TGameObjectFactory::ReadAbilityScores(nlohmann::json& json_game_object, TGameObjectId id)
{
    TAbilityScores result = TAbilityScores::FromJson(json_game_object, *this);
    ability_scores_.insert({id, [result]() { return result; }});
}


void TGameObjectFactory::ReadCreatureData(nlohmann::json& json_game_object, TGameObjectId id)
{
    creature_data_.insert({id, [this, json_game_object]() {
        return TCreatureData::FromJson(json_game_object, *this);
    }});
}


TProficiency TGameObjectFactory::ReadProficiency(nlohmann::json& json_game_object, int level)
{
    TProficiency proficiency(level);
    auto& json_proficiency = json_game_object["proficiency"];

    auto get_value = [](nlohmann::basic_json<> json_value) {
        TProficiency::Value value;
        if (json_value.is_number()) {
            value = json_value.get<int>();
        } else {
            value = ProficiencyLevelFromString(json_value.get<std::string>());
        }
        return value;
    };

    for (auto& [json_key, json_value] : json_proficiency["armor_category"].items()) {
        proficiency.SetProficiency(EArmorCategoryFromString(json_key), get_value(json_value));
    }

    for (auto& [json_key, json_value] : json_proficiency["weapon_category"].items()) {
        proficiency.SetProficiency(EWeaponCategoryFromString(json_key), get_value(json_value));
    }

    for (auto& [json_key, json_value] : json_proficiency["savethrow"].items()) {
        proficiency.SetProficiency(SavethrowFromString(json_key), get_value(json_value));
    }

    for (auto& [json_key, json_value] : json_proficiency["skill"].items()) {
        proficiency.SetProficiency(SkillFromString(json_key), get_value(json_value));
    }

    proficiency.SetProficiency(TPerceptionTag{}, get_value(json_proficiency["Perception"]));

    return proficiency;
}

void TGameObjectFactory::ReadCreature(nlohmann::json& json_game_object, TGameObjectId id)
{
    std::vector<TGameObjectId> actions;
    for (const auto& action : json_game_object["actions"]) {
        actions.emplace_back(TGameObjectIdManager::Instance().Register(action));
    }

    int level = json_game_object["creature_data"]["level"];
    TProficiency proficiency = ReadProficiency(json_game_object, level);

    std::vector<std::shared_ptr<TCreatureFeat>> feats;
    if (json_game_object.contains("feats")) {
        for (auto& feat_json : json_game_object["feats"]) {
            TPipelineReader reader;
            auto feat = std::make_shared<TCreatureFeat>();
            feat->name = feat_json["name"];
            const auto& block_json = feat_json["block"];
            if (block_json.is_string()) {
                feat->blocks.push_back(block_json.get<std::string>());
            } else {
                for (const auto& name : block_json) {
                    feat->blocks.push_back(name.get<std::string>());
                }
            }
            feat->pipeline = reader.ReadPipeline(feat_json["pipeline"]);
            feats.push_back(std::move(feat));
        }
    }

    creatures_.insert({id, [this, json_game_object, actions, proficiency, feats]() {
        TCreatureData data = TCreatureData::FromJson(json_game_object.at("creature_data"), *this);
        TCreature creature(std::move(data), proficiency);

        for (auto action_id : actions) {
            creature.AddAction(Create<TAction>(action_id));
        }

        for (const auto& feat : feats) {
            creature.AddFeat(feat);
        }

        return creature;
    }});
}


void TGameObjectFactory::ReadAction(nlohmann::json& json, TGameObjectId id)
{
    std::shared_ptr<TAction> action = std::make_shared<TAction>(action_reader_.ReadAction(json, *this));
    actions_.insert({id, [=]() {
        return action;
    }});
}


void TGameObjectFactory::ReadBattleMap(nlohmann::json& json, TGameObjectId id)
{
    TBattleMap battle_map(json);
    battle_maps_.insert({id, [=]() {
        return battle_map;
    }});
}

#include <game_object_factory.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>

#include <cpp_config.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "armor.h"
#include "battle_map.h"
#include "characteristics.h"
#include "game_object_id.h"
#include "hitpoints.h"
#include "proficiency.h"
#include "resources.h"
#include "weapon.h"

const std::string kPathToSchema = kRootDirPath + "/pf2e_engine/schemas/schema.json";

const std::unordered_map<std::string, TGameObjectFactory::FMethod>
TGameObjectFactory::kReaderMapping = {
    {"pf2e_armor", &TGameObjectFactory::ReadArmor},
    {"pf2e_weapon", &TGameObjectFactory::ReadWeapon},
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
    if (!std::filesystem::is_directory(source_path)) {
        throw std::runtime_error("not directory game object source is not supported yet");
    }

    for (auto de : std::filesystem::directory_iterator{source_path}) {
        if (de.is_regular_file()) {
            ReadObjectFromFile(de);
        }
    }
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

///////////////////////////////////////////////////////////////////////////////////////////////////

void TGameObjectFactory::ReadArmor(nlohmann::json& json_game_object, TGameObjectId id)
{
    TArmor result;
    result.ac_bonus_ = json_game_object["armor_class_bonus"];
    result.dex_cap_ = json_game_object["dexterity_cap"];
    result.category_ = ArmorCategoryFromString(json_game_object["category"]);

    armors_.insert({id, [result]() { return result; }});
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void TGameObjectFactory::ReadWeapon(nlohmann::json& json_game_object, TGameObjectId id)
{
    int base_die_size = json_game_object["base_die_size"];
    TDamage::Type damage_type = DamageTypeFromString(std::string{json_game_object["damage_type"]});
    EWeaponCategory category = WeaponCategoryFromString(json_game_object["category"]);
    std::string_view name = TGameObjectIdManager::Instance().Name(id);
    TWeapon result(name, base_die_size, damage_type, category);

    weapons_.insert({id, [result]() { return result; }});
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TResourcePool TGameObjectFactory::ReadCreatureResources(nlohmann::json& json_game_object)
{
    TResourcePool resource_pool;
    for (auto it = json_game_object.begin(); it != json_game_object.end(); ++it) {
        if (!it->is_number()) {
            std::stringstream ss;
            ss << "wrong resource count: \"" << *it << "\"";
            throw std::runtime_error(ss.str());
        }
        auto resource_id = TResourceIdManager::Instance().Register(it.key());
        resource_pool.Add(resource_id, *it);
    }
    return resource_pool;
}

TProficiency TGameObjectFactory::ReadProficiency(nlohmann::json& json_game_object)
{
    TProficiency proficiency(json_game_object["level"]);
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
        proficiency.SetProficiency(ArmorCategoryFromString(json_key), get_value(json_value));
    }

    for (auto& [json_key, json_value] : json_proficiency["weapon_category"].items()) {
        proficiency.SetProficiency(WeaponCategoryFromString(json_key), get_value(json_value));
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
    TCharacteristicSet stats([&]() {
        std::array<int, TCharacteristicSet::kCharacteristicCount> num_stats;
        const nlohmann::json& json_stats = json_game_object["characteristics"];
        for (size_t i = 0; i < TCharacteristicSet::kCharacteristicCount; ++i) {
            num_stats[i] = json_stats[ToString(static_cast<ECharacteristic>(i))];
        }
        return num_stats;
    }());

    TResourcePool resource_pool = ReadCreatureResources(json_game_object["resources"]);

    std::optional<TGameObjectId> armor_id;
    std::vector<std::pair<TGameObjectId, int>> weapon_ids;

    if (json_game_object.find("equipped") != json_game_object.end())
    {
        auto& equipped = json_game_object["equipped"];
        if (equipped.find("armor") != equipped.end()) {
            if (equipped["armor"].is_object()) {
                throw std::runtime_error("definition new armor in creature is not supported yet");
            }
            armor_id = TGameObjectIdManager::Instance().Register(equipped["armor"]);
        }

        for (const auto& weapon_json : equipped["weapons"]) {
            weapon_ids.emplace_back([&]() -> std::pair<TGameObjectId, int> {
                if (weapon_json["weapon"].is_object()) {
                    throw std::runtime_error("definition new weapon in creature is not supported yet");
                }
                return { TGameObjectIdManager::Instance().Register(weapon_json["weapon"]), weapon_json["grip"] };
            }());
        }
    }

    std::vector<TGameObjectId> actions;
    for (const auto& action : json_game_object["actions"]) {
        actions.emplace_back(TGameObjectIdManager::Instance().Register(action));
    }

    int race_hp = json_game_object["race_hitpoints"];
    int hp_per_level = json_game_object["hitpoint_per_level"];

    TProficiency proficiency = ReadProficiency(json_game_object);

    creatures_.insert({id, [this, armor_id, weapon_ids, resource_pool, stats, actions, race_hp, hp_per_level, proficiency]() {
        TArmor armor = armor_id ? this->Create<TArmor>(*armor_id) : TArmor{};
        THitPoints hp(race_hp + (hp_per_level + stats[ECharacteristic::Constitution].GetMod()) * proficiency.GetLevel());
        TCreature creature(stats, proficiency, armor, hp);
        creature.Resources() = resource_pool;

        auto hand_id = TResourceIdManager::Instance().Register("hand");

        for (auto [weapon_id, hand_count] : weapon_ids) {
            TWeapon weapon = this->Create<TWeapon>(weapon_id);
            assert(weapon.ValidGrip(hand_count));
            creature.Weapons().Equip({weapon, hand_count});

            if (!creature.Resources().HasResource(hand_id, hand_count)) {
                throw std::runtime_error("too many weapon, not enough hands");
            }
            creature.Resources().Reduce(hand_id, hand_count);
        }

        for (auto action_id : actions) {
            creature.AddAction(Create<TAction>(action_id));
        }

        return creature;
    }});
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void TGameObjectFactory::ReadAction(nlohmann::json& json, TGameObjectId id)
{
    std::shared_ptr<TAction> action = std::make_shared<TAction>(action_reader_.ReadAction(json));
    actions_.insert({id, [=]() {
        return action;
    }});
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void TGameObjectFactory::ReadBattleMap(nlohmann::json& json, TGameObjectId id)
{
    TBattleMap battle_map(json);
    battle_maps_.insert({id, [=]() {
        return battle_map;
    }});
}

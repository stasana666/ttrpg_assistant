#include <game_object_factory.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>

#include <cpp_config.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "battle_map.h"
#include "game_object_id.h"
#include "resources.h"

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
    static nlohmann::json_schema::json_validator validator{[]() {
        std::ifstream in(kPathToSchema);
        nlohmann::json schema = nlohmann::json::parse(in);
        return schema;
    }()};

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

    armors_.insert({id, [result]() { return result; }});
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void TGameObjectFactory::ReadWeapon(nlohmann::json& json_game_object, TGameObjectId id)
{
    int base_die_size = json_game_object["base_die_size"];
    TDamage::Type damage_type = DamageTypeFromString(std::string{json_game_object["damage_type"]});
    TWeapon result(base_die_size, damage_type);

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

    std::optional<TGameObjectId> armor_id = [&]() -> std::optional<TGameObjectId> {
        if (json_game_object.find("armor") == json_game_object.end()) {
            return std::nullopt;
        }
        if (json_game_object.is_object()) {
            throw std::runtime_error("definition new armor in creature is not supported yet");
        }
        return TGameObjectIdManager::Instance().Register(json_game_object["armor"]);
    }();

    std::vector<std::pair<TGameObjectId, int>> weapon_ids;
    for (const auto& weapon_json : json_game_object["weapons"]) {
        weapon_ids.emplace_back([&]() -> std::pair<TGameObjectId, int> {
            if (weapon_json["weapon"].is_object()) {
                throw std::runtime_error("definition new weapon in creature is not supported yet");
            }
            return { TGameObjectIdManager::Instance().Register(weapon_json["weapon"]), weapon_json["grip"] };
        }());
    }

    int max_hp = json_game_object["hitpoints"];

    creatures_.insert({id, [this, armor_id, weapon_ids, resource_pool, stats, max_hp]() {
        TArmor armor = armor_id ? this->CreateArmor(*armor_id) : TArmor{};
        TCreature creature(stats, armor, THitPoints(max_hp));
        creature.Resources() = resource_pool;

        auto hand_id = TResourceIdManager::Instance().Register("hand");

        for (auto [weapon_id, hand_count] : weapon_ids) {
            TWeapon weapon = this->CreateWeapon(weapon_id);
            assert(weapon.ValidGrip(hand_count));
            creature.Weapons().Equip({weapon, hand_count});

            if (!creature.Resources().HasResource(hand_id, hand_count)) {
                throw std::runtime_error("too many weapon, not enough hands");
            }
            creature.Resources().Reduce(hand_id, hand_count);
        }

        return creature;
    }});
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void TGameObjectFactory::ReadAction(nlohmann::json& json, TGameObjectId id)
{
    TAction action = action_reader_.ReadAction(json);
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

///////////////////////////////////////////////////////////////////////////////////////////////////

TArmor TGameObjectFactory::CreateArmor(TGameObjectId id) const
{
    return armors_.at(id)();
}

TWeapon TGameObjectFactory::CreateWeapon(TGameObjectId id) const
{
    return weapons_.at(id)();
}

TCreature TGameObjectFactory::CreateCreature(TGameObjectId id) const
{
    return creatures_.at(id)();
}

TAction TGameObjectFactory::CreateAction(TGameObjectId id) const
{
    return actions_.at(id)();
}

TBattleMap TGameObjectFactory::CreateBattleMap(TGameObjectId id) const
{
    return battle_maps_.at(id)();
}


#include <game_object_factory.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>

const std::unordered_map<std::string, TGameObjectFactory::FMethod>
TGameObjectFactory::kReaderMapping = {
    {"pf2e_armor", &TGameObjectFactory::ReadArmor},
    {"pf2e_weapon", &TGameObjectFactory::ReadWeapon},
    {"pf2e_creature", &TGameObjectFactory::ReadCreature},
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

    if (!json_game_object.is_object()) {
        throw std::runtime_error("not single object in file not supported yet");
    }

    auto reader = kReaderMapping.find(json_game_object["type"]);

    if (reader == kReaderMapping.end()) {
        std::stringstream ss;
        ss << "unknown type of game object " << json_game_object["type"];
        throw std::runtime_error(ss.str());
    }

    (this->*reader->second)(json_game_object);
}

TGameObjectId TGameObjectFactory::ReadGameObjectName(const nlohmann::json& json_game_object) const
{
    if (!json_game_object.contains("name")) {
        throw std::runtime_error("invalid game object, name not found");
    }
    std::string name = json_game_object["name"];
    return TGameObjectIdManager::Instance().Register(name);
}

void TGameObjectFactory::ReadArmor(const nlohmann::json& json_game_object)
{
    TGameObjectId id = ReadGameObjectName(json_game_object);
    const nlohmann::json& properties = json_game_object["properties"];

    TArmor result;
    result.ac_bonus_ = properties["armor_class_bonus"];
    result.dex_cap_ = properties["dexterity_cap"];

    armors_.insert({id, [result]() { return result; }});
}

void TGameObjectFactory::ReadWeapon(const nlohmann::json& json_game_object)
{
    TGameObjectId id = ReadGameObjectName(json_game_object);
    const nlohmann::json& properties = json_game_object["properties"];

    int base_die_size = properties["base_die_size"];
    TDamage::Type damage_type = DamageTypeFromString(std::string{properties["damage_type"]});
    TWeapon result(base_die_size, damage_type);

    weapons_.insert({id, [result]() { return result; }});
}

void TGameObjectFactory::ReadCreature(const nlohmann::json& json_game_object)
{
    TGameObjectId id = ReadGameObjectName(json_game_object);
    const nlohmann::json& properties = json_game_object["properties"];

    TCharacteristicSet stats([&]() {
        std::array<int, TCharacteristicSet::kCharacteristicCount> num_stats;
        const nlohmann::json& json_stats = properties["stats"];
        for (size_t i = 0; i < TCharacteristicSet::kCharacteristicCount; ++i) {
            num_stats[i] = json_stats[ToString(static_cast<ECharacteristic>(i))];
        }
        return num_stats;
    }());

    TGameObjectId armor_id = TGameObjectIdManager::Instance().Register(properties["armor"]);
    TGameObjectId weapon_id = TGameObjectIdManager::Instance().Register(properties["weapon"]);

    int max_hp = properties["hp"];

    creatures_.insert({id, [this, armor_id, weapon_id, stats, max_hp]() {
        TCreature result(stats, this->CreateArmor(armor_id), THitPoints(max_hp));
        return result;
    }});
}

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

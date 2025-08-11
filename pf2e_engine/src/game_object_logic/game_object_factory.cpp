#include <game_object_factory.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <variant>

const std::unordered_map<std::string, TGameObjectFactory::Method>
TGameObjectFactory::kReaderMapping = {
    {"armor", &TGameObjectFactory::ReadArmor}
};

void TGameObjectFactory::AddSource(std::filesystem::path source_path)
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

void TGameObjectFactory::ReadObjectFromFile(std::filesystem::path game_object_file)
{
    std::ifstream game_object_stream(game_object_file);
    nlohmann::json json_game_object = nlohmann::json::parse(game_object_stream);

    if (!json_game_object.is_object()) {
        throw std::runtime_error("not single object in file not supported yet");
    }

    auto reader = kReaderMapping.find(json_game_object["type"]);
    (this->*reader->second)(json_game_object);
}

void TGameObjectFactory::ReadArmor(const nlohmann::json& json_game_object)
{
    std::string name = json_game_object["name"];
    if (TGameObjectIdManager::Instance().Contains(name)) {
        std::stringstream ss;
        ss << "double declaration of game object with name: \"" << name << "\"";
        throw std::runtime_error(ss.str());
    }
    TGameObjectId id = TGameObjectIdManager::Instance().Register(name);

    const nlohmann::json& properties = json_game_object["properties"];

    TArmor result;
    result.ac_bonus_ = properties["ac_bonus"];
    result.dex_cap_ = properties["dex_cap"];

    game_objects_.Add(id, TGameObject(result));
}

TArmor TGameObjectFactory::CreateArmor(TGameObjectId id)
{
    TGameObject& object = game_objects_.GetRef(id);
    if (!std::holds_alternative<TArmor>(object)) {
        throw std::runtime_error("wrong type of object");
    }
    return std::get<TArmor>(object);
}

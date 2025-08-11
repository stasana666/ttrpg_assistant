#pragma once

#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object.h>
#include <pf2e_engine/game_object_logic/game_object_storage.h>

#include <nlohmann/json_fwd.hpp>

#include <filesystem>

class TGameObjectFactory {
public:
    TArmor CreateArmor(TGameObjectId id);

    void AddSource(std::filesystem::path source_path);

private:
    void ReadObjectFromFile(std::filesystem::path game_object_file);
    void ReadArmor(const nlohmann::json& json);

    using Method = void(TGameObjectFactory::*)(const nlohmann::json&);
    static const std::unordered_map<std::string, Method> kReaderMapping;

    TGameObjectStorage game_objects_;
};

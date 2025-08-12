#pragma once

#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object.h>
#include <pf2e_engine/game_object_logic/game_object_storage.h>

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <functional>

class TGameObjectFactory {
public:
    TArmor CreateArmor(TGameObjectId id) const;
    TWeapon CreateWeapon(TGameObjectId id) const;
    TCreature CreateCreature(TGameObjectId id) const;

    void AddSource(const std::filesystem::path& source_path);

private:
    void ReadObjectFromFile(const std::filesystem::path& game_object_file);

    void ValidateObject(nlohmann::json&) const;
    TGameObjectId ReadGameObjectName(nlohmann::json&) const;

    void ReadArmor(nlohmann::json&, TGameObjectId);
    void ReadWeapon(nlohmann::json&, TGameObjectId);
    void ReadCreature(nlohmann::json&, TGameObjectId);

    using FMethod = void(TGameObjectFactory::*)(nlohmann::json&, TGameObjectId);
    static const std::unordered_map<std::string, FMethod> kReaderMapping;

    template <class T>
    using FGameObjectFactory = std::function<T()>;

    template <class T>
    using TFactoryStorage = std::unordered_map<TGameObjectId, FGameObjectFactory<T>, TGameObjectIdHash>;

    TFactoryStorage<TArmor> armors_;
    TFactoryStorage<TWeapon> weapons_;
    TFactoryStorage<TCreature> creatures_;
};

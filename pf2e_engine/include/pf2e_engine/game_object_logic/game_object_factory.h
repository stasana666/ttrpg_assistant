#pragma once

#include <pf2e_engine/actions/action_reader.h>
#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object.h>
#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/material.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/inventory/creature_data.h>
#include <pf2e_engine/inventory/creature_parts.h>

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <functional>

class TGameObjectFactory {
public:
    template <class T>
    auto Create(TGameObjectId id) const;

    template <class T>
    std::vector<TGameObjectId> AllKnown() const;

    void AddSource(const std::filesystem::path& source_path);

    std::vector<TGameObjectId> Creatures() const;
    std::vector<TGameObjectId> BattleMaps() const;

private:
    using FMethod = void(TGameObjectFactory::*)(nlohmann::json&, TGameObjectId);
    static const std::unordered_map<std::string, FMethod> kReaderMapping;

    template <class T>
    using FGameObjectFactory = std::function<T()>;

    template <class T>
    using TFactoryStorage = std::unordered_map<TGameObjectId, FGameObjectFactory<T>, TGameObjectIdHash>;

    void ReadObjectFromFile(const std::filesystem::path& game_object_file);

    void ValidateObject(nlohmann::json&) const;
    TGameObjectId ReadGameObjectName(nlohmann::json&) const;
    TProficiency ReadProficiency(nlohmann::json& json_game_object);

    void ReadArmor(nlohmann::json&, TGameObjectId);
    void ReadMaterial(nlohmann::json&, TGameObjectId);
    void ReadWeapon(nlohmann::json&, TGameObjectId);
    void ReadRace(nlohmann::json&, TGameObjectId);
    void ReadClass(nlohmann::json&, TGameObjectId);
    void ReadAbilityScore(nlohmann::json&, TGameObjectId);
    void ReadAbilityScores(nlohmann::json&, TGameObjectId);
    void ReadCreatureData(nlohmann::json&, TGameObjectId);
    void ReadCreature(nlohmann::json&, TGameObjectId);
    void ReadAction(nlohmann::json&, TGameObjectId);
    void ReadBattleMap(nlohmann::json&, TGameObjectId);

    TProficiency ReadProficiency(nlohmann::json& json_game_object, int level);

    template <class T>
    const TFactoryStorage<T>& GetFactoryStorage() const;

    TActionReader action_reader_;

    TFactoryStorage<TArmor> armors_;
    TFactoryStorage<TMaterial> materials_;
    TFactoryStorage<TWeapon> weapons_;
    TFactoryStorage<TRace> races_;
    TFactoryStorage<TClass> classes_;
    TFactoryStorage<TAbilityScore> ability_score_;
    TFactoryStorage<TAbilityScores> ability_scores_;
    TFactoryStorage<TCreatureData> creature_data_;
    TFactoryStorage<TCreature> creatures_;
    TFactoryStorage<std::shared_ptr<TAction>> actions_;
    TFactoryStorage<TBattleMap> battle_maps_;
};


template <class T>
auto TGameObjectFactory::GetFactoryStorage() const -> const TFactoryStorage<T>&
{
    if constexpr (std::is_same_v<T, TArmor>) {
        return armors_;
    } else
    if constexpr (std::is_same_v<T, TMaterial>) {
        return materials_;
    } else
    if constexpr (std::is_same_v<T, TWeapon>) {
        return weapons_;
    } else
    if constexpr (std::is_same_v<T, TRace>) {
        return races_;
    } else
    if constexpr (std::is_same_v<T, TClass>) {
        return classes_;
    } else
    if constexpr (std::is_same_v<T, TAbilityScore>) {
        return ability_score_;
    } else
    if constexpr (std::is_same_v<T, TAbilityScores>) {
        return ability_scores_;
    } else
    if constexpr (std::is_same_v<T, TCreatureData>) {
        return creature_data_;
    } else
    if constexpr (std::is_same_v<T, TCreature>) {
        return creatures_;
    } else
    if constexpr (std::is_same_v<T, TAction>) {
        return actions_;
    } else
    if constexpr (std::is_same_v<T, TBattleMap>) {
        return battle_maps_;
    } else {
        static_assert(false);
    }
}


template <class T>
auto TGameObjectFactory::Create(TGameObjectId id) const
{
    if constexpr (std::is_same_v<T, TAction>) {
        return actions_.at(id)();
    } else {
        return GetFactoryStorage<T>().at(id)();
    }
}

template <class T>
std::vector<TGameObjectId> TGameObjectFactory::AllKnown() const
{
    std::vector<TGameObjectId> ids;
    for (auto& [key, value] : GetFactoryStorage<T>()) {
        ids.emplace_back(key);
    }
    return ids;
}

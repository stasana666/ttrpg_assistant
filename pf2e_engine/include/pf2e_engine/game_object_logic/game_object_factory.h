#pragma once

#include <pf2e_engine/actions/action_reader.h>
#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object.h>
#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/material.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/inventory/creature_parts.h>

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <functional>
#include <concepts>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <type_traits>
#include <utility>

class TGameObjectFactory {
public:
    template <class T>
    auto Create(TGameObjectId id) const;

    template <class T>
    std::vector<TGameObjectId> AllKnown() const;

    template <class T>
    void RegisterType(std::string json_type);

    void AddSource(const std::filesystem::path& source_path);

    std::vector<TGameObjectId> Creatures() const;
    std::vector<TGameObjectId> BattleMaps() const;
    std::shared_ptr<const TAbilityRules> AbilityRules() const;

private:
    class TGeneratedConstructionGuard {
    public:
        TGeneratedConstructionGuard(const TGameObjectFactory& factory, std::type_index type, TGameObjectId id);
        ~TGeneratedConstructionGuard();
        TGeneratedConstructionGuard(const TGeneratedConstructionGuard&) = delete;
        TGeneratedConstructionGuard& operator=(const TGeneratedConstructionGuard&) = delete;
    };

    using FMethod = void(TGameObjectFactory::*)(nlohmann::json&, TGameObjectId);
    static const std::unordered_map<std::string, FMethod> kReaderMapping;

    template <class T>
    using FGameObjectFactory = std::function<T(const TGameObjectFactory&)>;

    template <class T>
    using TFactoryStorage = std::unordered_map<TGameObjectId, FGameObjectFactory<T>, TGameObjectIdHash>;

    template <class T>
    using TCreatedObject = std::conditional_t<std::is_same_v<T, TAction>, std::shared_ptr<TAction>, T>;

    using TGeneratedDefinitions = std::unordered_map<
        TGameObjectId, std::shared_ptr<const nlohmann::json>, TGameObjectIdHash>;

    void ReadObjectFromFile(const std::filesystem::path& game_object_file);
    bool ReadGeneratedObject(const nlohmann::json& json);
    void RegisterGeneratedType(std::type_index type, std::string json_type);
    const nlohmann::json& GeneratedDefinition(std::type_index type, TGameObjectId id) const;
    void ValidateCreature(const TCreature& creature) const;
    void StoreCreature(nlohmann::json data, TGameObjectId id);

    void ValidateObject(nlohmann::json&) const;
    TGameObjectId ReadGameObjectName(nlohmann::json&) const;

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

    template <class T>
    const TFactoryStorage<TCreatedObject<T>>* GetFactoryStorage() const;

    TActionReader action_reader_;

    TFactoryStorage<TArmor> armors_;
    TFactoryStorage<TMaterial> materials_;
    TFactoryStorage<TWeapon> weapons_;
    TFactoryStorage<TRace> races_;
    TFactoryStorage<TClass> classes_;
    TFactoryStorage<TAbilityScore> ability_score_;
    TFactoryStorage<TAbilityScores> ability_scores_;
    TFactoryStorage<std::shared_ptr<TAction>> actions_;
    struct TAbilityBinding {
        std::string Definition;
        std::optional<ETrigger> Trigger;
        bool RequiresWeaponSource;
    };
    std::unordered_map<TGameObjectId, TAbilityBinding, TGameObjectIdHash> ability_bindings_;
    std::map<std::string, std::shared_ptr<const TCreatureFeat>, std::less<>> feature_hooks_;
    mutable std::shared_ptr<const TAbilityRules> ability_rules_;
    TFactoryStorage<TBattleMap> battle_maps_;
    std::unordered_map<std::string, std::type_index> generated_types_;
    std::unordered_map<std::type_index, TGeneratedDefinitions> generated_definitions_;
};


template <class T>
auto TGameObjectFactory::GetFactoryStorage() const -> const TFactoryStorage<TCreatedObject<T>>*
{
    if constexpr (std::is_same_v<T, TArmor>) {
        return &armors_;
    } else
    if constexpr (std::is_same_v<T, TMaterial>) {
        return &materials_;
    } else
    if constexpr (std::is_same_v<T, TWeapon>) {
        return &weapons_;
    } else
    if constexpr (std::is_same_v<T, TRace>) {
        return &races_;
    } else
    if constexpr (std::is_same_v<T, TClass>) {
        return &classes_;
    } else
    if constexpr (std::is_same_v<T, TAbilityScore>) {
        return &ability_score_;
    } else
    if constexpr (std::is_same_v<T, TAbilityScores>) {
        return &ability_scores_;
    } else
    if constexpr (std::is_same_v<T, TAction>) {
        return &actions_;
    } else
    if constexpr (std::is_same_v<T, TBattleMap>) {
        return &battle_maps_;
    } else {
        return nullptr;
    }
}


template <class T>
auto TGameObjectFactory::Create(TGameObjectId id) const
{
    if (const auto* storage = GetFactoryStorage<T>()) {
        return storage->at(id)(*this);
    }
    if constexpr (requires(const nlohmann::json& json, const TGameObjectFactory& factory) {
        { T::FromJson(json, factory) } -> std::same_as<T>;
    }) {
        const auto& definition = GeneratedDefinition(typeid(T), id);
        TGeneratedConstructionGuard guard(*this, typeid(T), id);
        T result = T::FromJson(definition, *this);
        if constexpr (std::is_same_v<T, TCreature>) {
            ValidateCreature(result);
        }
        return result;
    } else {
        throw std::runtime_error("type has no factory or FromJson loader");
    }
}

template <class T>
std::vector<TGameObjectId> TGameObjectFactory::AllKnown() const
{
    std::vector<TGameObjectId> ids;
    if (const auto* storage = GetFactoryStorage<T>()) {
        for (const auto& [key, value] : *storage) {
            ids.emplace_back(key);
        }
    } else if (auto definitions = generated_definitions_.find(typeid(T));
               definitions != generated_definitions_.end()) {
        for (const auto& [key, value] : definitions->second) {
            ids.emplace_back(key);
        }
    }
    return ids;
}

template <class T>
void TGameObjectFactory::RegisterType(std::string json_type)
{
    static_assert(requires(const nlohmann::json& json, const TGameObjectFactory& factory) {
        { T::FromJson(json, factory) } -> std::same_as<T>;
    });
    if (GetFactoryStorage<T>() != nullptr) {
        throw std::invalid_argument("type already uses a built-in factory");
    }
    RegisterGeneratedType(typeid(T), std::move(json_type));
}

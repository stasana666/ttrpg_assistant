// AUTO-GENERATED FROM computed.ttrpg -- DO NOT EDIT.
// Source of truth: pf2e_engine/data/schemas/computed.ttrpg
#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/guarded.h>

#include <nlohmann/json_fwd.hpp>

#include <limits>
#include <string>
#include <utility>
#include <pf2e_engine/common/bounded_quantity.h>

class TGameObjectFactory;

class TPart {
public:
    const int& Bonus() const { return Bonus_; }
    TGuarded<int> Bonus() { return TGuarded<int>(Bonus_); }

    static TPart FromJson(const nlohmann::json& j, const TGameObjectFactory& factory);
    TAstNode GetAst(TAstContext& ctx) const;
    static void RegisterDslProperties();

private:
    int Bonus_{};
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TPart> : std::true_type {};

class TAbility {
public:
    const int& Value() const { return Value_; }
    TGuarded<int> Value() { return TGuarded<int>(Value_); }
    int Modifier() const { return ((Value_ - 10) / 2); }

    static TAbility FromJson(const nlohmann::json& j, const TGameObjectFactory& factory);
    TAstNode GetAst(TAstContext& ctx) const;
    static void RegisterDslProperties();

private:
    int Value_{};
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TAbility> : std::true_type {};

class TStats {
public:
    const TBoundedQuantity& Health() const { return Health_; }
    TGuarded<TBoundedQuantity> Health() { return TGuarded<TBoundedQuantity>(Health_); }
    const int& Base() const { return Base_; }
    TGuarded<int> Base() { return TGuarded<int>(Base_); }
    const int& Level() const { return Level_; }
    TGuarded<int> Level() { return TGuarded<int>(Level_); }
    const int& PerLevel() const { return PerLevel_; }
    TGuarded<int> PerLevel() { return TGuarded<int>(PerLevel_); }
    const int& Total() const { return Total_; }
    TGuarded<int> Total() { return TGuarded<int>(Total_); }
    const TPart& Part() const { return Part_; }
    TGuarded<TPart> Part() { return TGuarded<TPart>(Part_); }
    const int& Boosted() const { return Boosted_; }
    TGuarded<int> Boosted() { return TGuarded<int>(Boosted_); }
    const TAbility& Ability() const { return Ability_; }
    TGuarded<TAbility> Ability() { return TGuarded<TAbility>(Ability_); }
    const int& ModBoost() const { return ModBoost_; }
    TGuarded<int> ModBoost() { return TGuarded<int>(ModBoost_); }

    static TStats FromJson(const nlohmann::json& j, const TGameObjectFactory& factory);
    TAstNode GetAst(TAstContext& ctx) const;
    static void RegisterDslProperties();

private:
    TBoundedQuantity Health_{};
    int Base_{};
    int Level_{1};
    int PerLevel_{};
    int Total_{};
    TPart Part_{};
    int Boosted_{};
    TAbility Ability_{};
    int ModBoost_{};
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TStats> : std::true_type {};


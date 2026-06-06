// AUTO-GENERATED FROM computed.ttrpg -- DO NOT EDIT.
// Source of truth: pf2e_engine/data/schemas/computed.ttrpg
#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>

#include <nlohmann/json_fwd.hpp>

#include <limits>
#include <string>
#include <pf2e_engine/common/bounded_quantity.h>

class TGameObjectFactory;

class TPart {
public:
    int Bonus() const { return Bonus_; }

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
    int Value() const { return Value_; }
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
    TBoundedQuantity Health() const { return Health_; }
    int Base() const { return Base_; }
    int Level() const { return Level_; }
    int PerLevel() const { return PerLevel_; }
    int Total() const { return Total_; }
    TPart Part() const { return Part_; }
    int Boosted() const { return Boosted_; }
    TAbility Ability() const { return Ability_; }
    int ModBoost() const { return ModBoost_; }

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


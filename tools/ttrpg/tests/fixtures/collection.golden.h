// AUTO-GENERATED FROM collection.ttrpg -- DO NOT EDIT.
// Source of truth: pf2e_engine/data/schemas/collection.ttrpg
#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>

#include <nlohmann/json_fwd.hpp>

#include <limits>
#include <string>
#include <pf2e_engine/common/id_collection.h>

class TGameObjectFactory;

class TThing {
public:
    int X() const { return X_; }

    static TThing FromJson(const nlohmann::json& j, const TGameObjectFactory& factory);
    TAstNode GetAst(TAstContext& ctx) const;
    static void RegisterDslProperties();

private:
    int X_{};
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TThing> : std::true_type {};

class THolder {
public:
    int Count() const { return Count_; }
    const TIdCollection<TThing>& Things() const { return Things_; }
    TIdCollection<TThing>& Things() { return Things_; }

    static THolder FromJson(const nlohmann::json& j, const TGameObjectFactory& factory);
    TAstNode GetAst(TAstContext& ctx) const;
    static void RegisterDslProperties();

private:
    int Count_{0};
    TIdCollection<TThing> Things_{};
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<THolder> : std::true_type {};


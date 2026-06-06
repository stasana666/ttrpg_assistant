// AUTO-GENERATED FROM basic.ttrpg -- DO NOT EDIT.
// Source of truth: pf2e_engine/data/schemas/basic.ttrpg
#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>

#include <nlohmann/json_fwd.hpp>

#include <limits>
#include <string>
#include <set>
#include <pf2e_engine/common/bounded_quantity.h>

class TGameObjectFactory;

enum class EColor {
    Red,
    Green,
    Blue,
};

std::string ToString(EColor v);
EColor EColorFromString(const std::string& s);

class TThing {
public:
    std::string Name() const { return Name_; }
    int Count() const { return Count_; }
    EColor Color() const { return Color_; }
    TBoundedQuantity Charges() const { return Charges_; }
    const std::set<EColor>& Tags() const { return Tags_; }

    static TThing FromJson(const nlohmann::json& j, const TGameObjectFactory& factory);
    TAstNode GetAst(TAstContext& ctx) const;
    static void RegisterDslProperties();

private:
    std::string Name_{};
    int Count_{3};
    EColor Color_{};
    TBoundedQuantity Charges_{};
    std::set<EColor> Tags_{};
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TThing> : std::true_type {};


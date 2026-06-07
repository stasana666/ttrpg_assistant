// AUTO-GENERATED FROM basic.ttrpg -- DO NOT EDIT.
// Source of truth: pf2e_engine/data/schemas/basic.ttrpg
#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/guarded.h>

#include <nlohmann/json_fwd.hpp>

#include <limits>
#include <string>
#include <utility>
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
    const std::string& Name() const { return Name_; }
    TGuarded<std::string> Name() { return TGuarded<std::string>(Name_); }
    const int& Count() const { return Count_; }
    TGuarded<int> Count() { return TGuarded<int>(Count_); }
    const EColor& Color() const { return Color_; }
    TGuarded<EColor> Color() { return TGuarded<EColor>(Color_); }
    const TBoundedQuantity& Charges() const { return Charges_; }
    TGuarded<TBoundedQuantity> Charges() { return TGuarded<TBoundedQuantity>(Charges_); }
    const std::set<EColor>& Tags() const { return Tags_; }
    TGuarded<std::set<EColor>> Tags() { return TGuarded<std::set<EColor>>(Tags_); }

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


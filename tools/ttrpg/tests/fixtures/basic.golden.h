// AUTO-GENERATED FROM basic.ttrpg -- DO NOT EDIT.
// Source of truth: pf2e_engine/data/schemas/basic.ttrpg
#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/ability.h>
#include <pf2e_engine/common/guarded.h>

#include <limits>
#include <string>
#include <utility>
#include <optional>
#include <pf2e_engine/common/rule_integer.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/common/generated_construction.h>
#include <pf2e_engine/common/rule_integer.h>
#include <algorithm>
#include <type_traits>
#include <cstdint>
#include <set>
#include <pf2e_engine/common/bounded_quantity.h>

class TThing;

enum class EColor {
    Red,
    Green,
    Blue,
};

std::string ToString(EColor v);
EColor EColorFromString(const std::string& s);

class TThing {
public:
    template <class TVisitor>
    void VisitAbilitySources(TVisitor& visitor, const TAbilitySourcePath& path = {}) const {
        if constexpr (requires { static_cast<bool>(visitor.ShouldVisit(path)); }) {
            if (!visitor.ShouldVisit(path)) { return; }
        }
        visitor(*this, path);
    }
    struct TInitialValues {
        std::optional<std::string> Name{};
        std::optional<int> Count{};
        std::optional<EColor> Color{};
        std::optional<TBoundedQuantity> Charges{};
        std::optional<std::set<EColor>> Tags{};
    };

    TThing();
    static TThing Create(const TInitialValues& initial);
    static TInitialValues ResolveInitialValues(const TInitialValues& initial);
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

    TAstNode GetAst(TAstContext& ctx) const;
    void VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source = {}) const;

private:
    friend struct TGeneratedConstruction;
    struct TUninitialized {};
    explicit TThing(TUninitialized, std::string value_Name = TGeneratedConstruction::Blank<std::string>(), int value_Count = TGeneratedConstruction::Blank<int>(), EColor value_Color = TGeneratedConstruction::Blank<::EColor>(), TBoundedQuantity value_Charges = TGeneratedConstruction::Blank<TBoundedQuantity>(), std::set<EColor> value_Tags = TGeneratedConstruction::Blank<std::set<::EColor>>()) : Name_(std::move(value_Name)), Count_(std::move(value_Count)), Color_(std::move(value_Color)), Charges_(std::move(value_Charges)), Tags_(std::move(value_Tags)) {}
    std::string Name_;
    int Count_;
    EColor Color_;
    TBoundedQuantity Charges_;
    std::set<EColor> Tags_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TThing> : std::true_type {};

inline void TThing::VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source) const {
    (void)collector;
    (void)source;
}

void RegisterBasicAbilities(TAbilityCatalog& catalog);
struct TAbilityRules;
void RegisterBasicRules(TAbilityRules& rules);

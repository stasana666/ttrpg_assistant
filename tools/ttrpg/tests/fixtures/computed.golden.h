// AUTO-GENERATED FROM computed.ttrpg -- DO NOT EDIT.
// Source of truth: pf2e_engine/data/schemas/computed.ttrpg
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
#include <pf2e_engine/common/bounded_quantity.h>

class TPart;
class TAbility;
class TStats;

class TPart {
public:
    template <class TVisitor>
    void VisitAbilitySources(TVisitor& visitor, const TAbilitySourcePath& path = {}) const {
        if constexpr (requires { static_cast<bool>(visitor.ShouldVisit(path)); }) {
            if (!visitor.ShouldVisit(path)) { return; }
        }
        visitor(*this, path);
    }
    struct TInitialValues {
        std::optional<int> Bonus{};
    };

    TPart();
    static TPart Create(const TInitialValues& initial);
    static TInitialValues ResolveInitialValues(const TInitialValues& initial);
    const int& Bonus() const { return Bonus_; }
    TGuarded<int> Bonus() { return TGuarded<int>(Bonus_); }

    TAstNode GetAst(TAstContext& ctx) const;
    void VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source = {}) const;

private:
    friend struct TGeneratedConstruction;
    struct TUninitialized {};
    explicit TPart(TUninitialized, int value_Bonus = TGeneratedConstruction::Blank<int>()) : Bonus_(std::move(value_Bonus)) {}
    int Bonus_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TPart> : std::true_type {};

inline void TPart::VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source) const {
    (void)collector;
    (void)source;
}

class TAbility {
public:
    template <class TVisitor>
    void VisitAbilitySources(TVisitor& visitor, const TAbilitySourcePath& path = {}) const {
        if constexpr (requires { static_cast<bool>(visitor.ShouldVisit(path)); }) {
            if (!visitor.ShouldVisit(path)) { return; }
        }
        visitor(*this, path);
    }
    struct TInitialValues {
        std::optional<int> Value{};
    };

    TAbility();
    static TAbility Create(const TInitialValues& initial);
    static TInitialValues ResolveInitialValues(const TInitialValues& initial);
    const int& Value() const { return Value_; }
    TGuarded<int> Value() { return TGuarded<int>(Value_); }
    int Modifier() const;

    TAstNode GetAst(TAstContext& ctx) const;
    void VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source = {}) const;

private:
    friend struct TGeneratedConstruction;
    struct TUninitialized {};
    explicit TAbility(TUninitialized, int value_Value = TGeneratedConstruction::Blank<int>()) : Value_(std::move(value_Value)) {}
    int Value_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TAbility> : std::true_type {};

inline void TAbility::VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source) const {
    (void)collector;
    (void)source;
}

class TStats {
public:
    template <class TVisitor>
    void VisitAbilitySources(TVisitor& visitor, const TAbilitySourcePath& path = {}) const {
        if constexpr (requires { static_cast<bool>(visitor.ShouldVisit(path)); }) {
            if (!visitor.ShouldVisit(path)) { return; }
        }
        visitor(*this, path);
        Part_.VisitAbilitySources(visitor, path.Field("part"));
        Ability_.VisitAbilitySources(visitor, path.Field("ability"));
    }
    struct TInitialValues {
        std::optional<TBoundedQuantity> Health{};
        std::optional<int> Base{};
        std::optional<int> Level{};
        std::optional<int> PerLevel{};
        std::optional<int> Total{};
        std::optional<TPart> Part{};
        std::optional<int> Boosted{};
        std::optional<TAbility> Ability{};
        std::optional<int> ModBoost{};
    };

    TStats();
    static TStats Create(const TInitialValues& initial);
    static TInitialValues ResolveInitialValues(const TInitialValues& initial);
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

    TAstNode GetAst(TAstContext& ctx) const;
    void VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source = {}) const;

private:
    friend struct TGeneratedConstruction;
    struct TUninitialized {};
    explicit TStats(TUninitialized, TBoundedQuantity value_Health = TGeneratedConstruction::Blank<TBoundedQuantity>(), int value_Base = TGeneratedConstruction::Blank<int>(), int value_Level = TGeneratedConstruction::Blank<int>(), int value_PerLevel = TGeneratedConstruction::Blank<int>(), int value_Total = TGeneratedConstruction::Blank<int>(), TPart value_Part = TGeneratedConstruction::Blank<::TPart>(), int value_Boosted = TGeneratedConstruction::Blank<int>(), TAbility value_Ability = TGeneratedConstruction::Blank<::TAbility>(), int value_ModBoost = TGeneratedConstruction::Blank<int>()) : Health_(std::move(value_Health)), Base_(std::move(value_Base)), Level_(std::move(value_Level)), PerLevel_(std::move(value_PerLevel)), Total_(std::move(value_Total)), Part_(std::move(value_Part)), Boosted_(std::move(value_Boosted)), Ability_(std::move(value_Ability)), ModBoost_(std::move(value_ModBoost)) {}
    TBoundedQuantity Health_;
    int Base_;
    int Level_;
    int PerLevel_;
    int Total_;
    TPart Part_;
    int Boosted_;
    TAbility Ability_;
    int ModBoost_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TStats> : std::true_type {};

inline void TStats::VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source) const {
    (void)collector;
    (void)source;
}

void RegisterComputedAbilities(TAbilityCatalog& catalog);
struct TAbilityRules;
void RegisterComputedRules(TAbilityRules& rules);

// AUTO-GENERATED FROM collection.ttrpg -- DO NOT EDIT.
// Source of truth: pf2e_engine/data/schemas/collection.ttrpg
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
#include <pf2e_engine/common/id_collection.h>

class TThing;
class THolder;

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
        std::optional<int> X{};
    };

    TThing();
    static TThing Create(const TInitialValues& initial);
    static TInitialValues ResolveInitialValues(const TInitialValues& initial);
    const int& X() const { return X_; }
    TGuarded<int> X() { return TGuarded<int>(X_); }

    TAstNode GetAst(TAstContext& ctx) const;
    void VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source = {}) const;

private:
    friend struct TGeneratedConstruction;
    struct TUninitialized {};
    explicit TThing(TUninitialized, int value_X = TGeneratedConstruction::Blank<int>()) : X_(std::move(value_X)) {}
    int X_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TThing> : std::true_type {};

inline void TThing::VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source) const {
    (void)collector;
    (void)source;
}

class THolder {
public:
    template <class TVisitor>
    void VisitAbilitySources(TVisitor& visitor, const TAbilitySourcePath& path = {}) const {
        if constexpr (requires { static_cast<bool>(visitor.ShouldVisit(path)); }) {
            if (!visitor.ShouldVisit(path)) { return; }
        }
        visitor(*this, path);
        for (auto entry : Things_) {
            entry->VisitAbilitySources(visitor, path.Field("things").Entry(entry.Id().Value));
        }
    }
    struct TInitialValues {
        std::optional<int> Count{};
        std::optional<TIdCollection<TThing>> Things{};
    };

    THolder();
    static THolder Create(const TInitialValues& initial);
    static TInitialValues ResolveInitialValues(const TInitialValues& initial);
    const int& Count() const { return Count_; }
    TGuarded<int> Count() { return TGuarded<int>(Count_); }
    const TIdCollection<TThing>& Things() const { return Things_; }
    TGuarded<TIdCollection<TThing>> Things() { return TGuarded<TIdCollection<TThing>>(Things_); }

    TAstNode GetAst(TAstContext& ctx) const;
    void VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source = {}) const;

private:
    friend struct TGeneratedConstruction;
    struct TUninitialized {};
    explicit THolder(TUninitialized, int value_Count = TGeneratedConstruction::Blank<int>(), TIdCollection<TThing> value_Things = TGeneratedConstruction::Blank<TIdCollection<::TThing>>()) : Count_(std::move(value_Count)), Things_(std::move(value_Things)) {}
    int Count_;
    TIdCollection<TThing> Things_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<THolder> : std::true_type {};

inline void THolder::VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source) const {
    (void)collector;
    (void)source;
}

void RegisterCollectionAbilities(TAbilityCatalog& catalog);
struct TAbilityRules;
void RegisterCollectionRules(TAbilityRules& rules);

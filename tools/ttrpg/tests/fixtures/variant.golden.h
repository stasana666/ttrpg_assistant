// AUTO-GENERATED FROM variant.ttrpg -- DO NOT EDIT.
// Source of truth: pf2e_engine/data/schemas/variant.ttrpg
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
#include <type_traits>
#include <utility>
#include <variant>
#include <pf2e_engine/common/variant_map.h>

class TSpell;

enum class EColor {
    Red,
    Green,
};

std::string ToString(EColor v);
EColor EColorFromString(const std::string& s);

enum class EEffectKind {
    Stun,
    Burn,
    Tint,
    Zone,
};

std::string ToString(EEffectKind v);
EEffectKind EEffectKindFromString(const std::string& s);

struct TEffectStun {
    static constexpr auto Kind = EEffectKind::Stun;
};

struct TEffectBurn {
    static constexpr auto Kind = EEffectKind::Burn;
    int Damage{};
};

struct TEffectTint {
    static constexpr auto Kind = EEffectKind::Tint;
    EColor Color{};
};

struct TEffectZone {
    static constexpr auto Kind = EEffectKind::Zone;
    int Radius{1};
    EColor Color{};
};

class TEffect {
public:
    using TPayload = std::variant<
        TEffectStun,
        TEffectBurn,
        TEffectTint,
        TEffectZone>;

    TEffect() = default;
    template <class T>
        requires (!std::is_same_v<std::decay_t<T>, TEffect>)
    TEffect(T alt) : Payload_(std::move(alt)) {}

    EEffectKind Kind() const {
        return std::visit(
            [](const auto& a) { return std::decay_t<decltype(a)>::Kind; }, Payload_);
    }
    const TPayload& Payload() const { return Payload_; }

    template <class T> const T* TryGet() const { return std::get_if<T>(&Payload_); }

    TAstNode GetAst(TAstContext& ctx) const;

private:
    TPayload Payload_;
};

template <>
struct TIsAstRecursive<TEffect> : std::true_type {};

class TSpell {
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
        std::optional<TVariantMap<EEffectKind, TEffect>> Effects{};
    };

    TSpell();
    static TSpell Create(const TInitialValues& initial);
    static TInitialValues ResolveInitialValues(const TInitialValues& initial);
    const std::string& Name() const { return Name_; }
    TGuarded<std::string> Name() { return TGuarded<std::string>(Name_); }
    const TVariantMap<EEffectKind, TEffect>& Effects() const { return Effects_; }
    TGuarded<TVariantMap<EEffectKind, TEffect>> Effects() { return TGuarded<TVariantMap<EEffectKind, TEffect>>(Effects_); }

    TAstNode GetAst(TAstContext& ctx) const;
    void VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source = {}) const;

private:
    friend struct TGeneratedConstruction;
    struct TUninitialized {};
    explicit TSpell(TUninitialized, std::string value_Name = TGeneratedConstruction::Blank<std::string>(), TVariantMap<EEffectKind, TEffect> value_Effects = TGeneratedConstruction::Blank<TVariantMap<EEffectKind, ::TEffect>>()) : Name_(std::move(value_Name)), Effects_(std::move(value_Effects)) {}
    std::string Name_;
    TVariantMap<EEffectKind, TEffect> Effects_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TSpell> : std::true_type {};

inline void TSpell::VisitGrantedAbilities(TAbilityCollector& collector, const TAbilitySourcePath& source) const {
    (void)collector;
    (void)source;
}

void RegisterVariantAbilities(TAbilityCatalog& catalog);
struct TAbilityRules;
void RegisterVariantRules(TAbilityRules& rules);

// AUTO-GENERATED FROM variant.ttrpg -- DO NOT EDIT.
#include <variant.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/actions/ability.h>

#include <stdexcept>
#include <string>
#include <utility>

std::string ToString(EColor v) {
    switch (v) {
        case EColor::Red: return "Red";
        case EColor::Green: return "Green";
    }
    throw std::runtime_error("invalid EColor value");
}

EColor EColorFromString(const std::string& s) {
    if (s == "Red") {
        return EColor::Red;
    }
    if (s == "Green") {
        return EColor::Green;
    }
    throw std::runtime_error("unknown EColor: \"" + s + "\"");
}

std::string ToString(EEffectKind v) {
    switch (v) {
        case EEffectKind::Stun: return "Stun";
        case EEffectKind::Burn: return "Burn";
        case EEffectKind::Tint: return "Tint";
        case EEffectKind::Zone: return "Zone";
    }
    throw std::runtime_error("invalid EEffectKind value");
}

EEffectKind EEffectKindFromString(const std::string& s) {
    if (s == "Stun") {
        return EEffectKind::Stun;
    }
    if (s == "Burn") {
        return EEffectKind::Burn;
    }
    if (s == "Tint") {
        return EEffectKind::Tint;
    }
    if (s == "Zone") {
        return EEffectKind::Zone;
    }
    throw std::runtime_error("unknown EEffectKind: \"" + s + "\"");
}

TAstNode TEffect::GetAst([[maybe_unused]] TAstContext& ctx) const {
    TAstNode node = TAstNode::MakeObject("TEffect");
    AddValueField(node, "kind", Kind());
    switch (Kind()) {
        case EEffectKind::Stun: {
            break;
        }
        case EEffectKind::Burn: {
            const auto& a = std::get<TEffectBurn>(Payload_);
            AddValueField(node, "damage", a.Damage);
            break;
        }
        case EEffectKind::Tint: {
            const auto& a = std::get<TEffectTint>(Payload_);
            AddValueField(node, "color", a.Color);
            break;
        }
        case EEffectKind::Zone: {
            const auto& a = std::get<TEffectZone>(Payload_);
            AddValueField(node, "radius", a.Radius);
            AddValueField(node, "color", a.Color);
            break;
        }
    }
    return node;
}

TSpell::TSpell() : TSpell(Create({})) {}

TSpell::TInitialValues TSpell::ResolveInitialValues(const TInitialValues& initial) {
    TSpell result(TUninitialized{}, initial.Name ? *initial.Name : TGeneratedConstruction::Blank<std::string>(), initial.Effects ? *initial.Effects : TGeneratedConstruction::Blank<TVariantMap<EEffectKind, ::TEffect>>());
    [[maybe_unused]] auto& self = result;
    (void)initial;
    result.Name_ = initial.Name ? *initial.Name : std::string{};
    result.Effects_ = initial.Effects ? *initial.Effects : TVariantMap<EEffectKind, TEffect>{};
    return TInitialValues{.Name = std::move(result.Name_), .Effects = std::move(result.Effects_), };
}

TSpell TSpell::Create(const TInitialValues& initial) {
    [[maybe_unused]] auto values = ResolveInitialValues(initial);
    TSpell result(TUninitialized{}, std::move(*values.Name), std::move(*values.Effects));
    return result;
}

TAstNode TSpell::GetAst([[maybe_unused]] TAstContext& ctx) const {
    TAstNode node = TAstNode::MakeObject("TSpell");
    [[maybe_unused]] const std::string owner_identity = ctx.IdentityOf(this);
    AddValueField(node, "name", Name_);
    {
        TAstNode container = TAstNode::MakeObject("container");
        for (const auto& [key, value] : Effects_) {
            if (!owner_identity.empty()) {
                ctx.RegisterIdentity(&value, owner_identity + ".effects." + ToString(key));
            }
            AddOwnedObject(container, ToString(key), value, ctx);
        }
        node.AddChild("effects", std::move(container));
    }
    return node;
}

void RegisterVariantAbilities(TAbilityCatalog& catalog) {
    (void)catalog;
}

void RegisterVariantRules(TAbilityRules& rules) {
    RegisterVariantAbilities(rules.Catalog);
}


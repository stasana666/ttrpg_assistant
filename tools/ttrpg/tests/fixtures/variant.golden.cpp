// AUTO-GENERATED FROM variant.ttrpg -- DO NOT EDIT.
#include <variant.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/dsl/property_registry.h>
#include <pf2e_engine/dsl/value.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

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

TEffect TEffect::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory) {
    (void)factory;
    if (j.is_string()) {
        switch (EEffectKindFromString(j.get<std::string>())) {
            case EEffectKind::Stun: return TEffectStun{};
            default:
                throw std::runtime_error("parameterized TEffect given without parameters");
        }
    }
    if (j.is_object() && j.size() == 1) {
        auto it = j.begin();
        const std::string& key = it.key();
        const nlohmann::json& val = it.value();
        switch (EEffectKindFromString(key)) {
            case EEffectKind::Burn: {
                TEffectBurn a;
                a.Damage = val.get<int>();
                return a;
            }
            case EEffectKind::Tint: {
                TEffectTint a;
                a.Color = EColorFromString(val.get<std::string>());
                return a;
            }
            case EEffectKind::Zone: {
                TEffectZone a;
                a.Radius = val.value("radius", int{1});
                a.Color = EColorFromString(val.at("color").get<std::string>());
                return a;
            }
            default:
                throw std::runtime_error("flag TEffect given with parameters");
        }
    }
    throw std::runtime_error("malformed TEffect");
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

TSpell TSpell::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory) {
    TSpell r;
    r.Name_ = j.at("name").get<std::string>();
    if (j.contains("effects")) {
        for (const auto& item : j.at("effects")) {
            r.Effects_.Set(TEffect::FromJson(item, factory));
        }
    }
    return r;
}

TAstNode TSpell::GetAst([[maybe_unused]] TAstContext& ctx) const {
    TAstNode node = TAstNode::MakeObject("TSpell");
    AddValueField(node, "name", Name_);
    {
        TAstNode set_node = TAstNode::MakeObject("container");
        for (const auto& [kind, value] : Effects_) {
            AddOwnedObject(set_node, ToString(kind), value, ctx);
        }
        node.AddChild("effects", std::move(set_node));
    }
    return node;
}

void TSpell::RegisterDslProperties() {
    auto& r = TPropertyRegistry<TSpell>::Instance();
    (void)r;
}


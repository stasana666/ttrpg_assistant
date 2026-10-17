// AUTO-GENERATED FROM basic.ttrpg -- DO NOT EDIT.
#include <basic.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/actions/ability.h>

#include <stdexcept>
#include <string>
#include <utility>

std::string ToString(EColor v) {
    switch (v) {
        case EColor::Red: return "Red";
        case EColor::Green: return "Green";
        case EColor::Blue: return "Blue";
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
    if (s == "Blue") {
        return EColor::Blue;
    }
    throw std::runtime_error("unknown EColor: \"" + s + "\"");
}

TThing::TThing() : TThing(Create({})) {}

TThing::TInitialValues TThing::ResolveInitialValues(const TInitialValues& initial) {
    TThing result(TUninitialized{}, initial.Name ? *initial.Name : TGeneratedConstruction::Blank<std::string>(), initial.Count ? *initial.Count : TGeneratedConstruction::Blank<int>(), initial.Color ? *initial.Color : TGeneratedConstruction::Blank<::EColor>(), initial.Charges ? *initial.Charges : TGeneratedConstruction::Blank<TBoundedQuantity>(), initial.Tags ? *initial.Tags : TGeneratedConstruction::Blank<std::set<::EColor>>());
    [[maybe_unused]] auto& self = result;
    (void)initial;
    result.Name_ = initial.Name ? *initial.Name : std::string{};
    result.Count_ = initial.Count ? *initial.Count : 3;
    result.Color_ = initial.Color ? *initial.Color : EColor{};
    result.Charges_ = initial.Charges ? *initial.Charges : TBoundedQuantity{};
    result.Tags_ = initial.Tags ? *initial.Tags : std::set<EColor>{};
    return TInitialValues{.Name = std::move(result.Name_), .Count = std::move(result.Count_), .Color = std::move(result.Color_), .Charges = std::move(result.Charges_), .Tags = std::move(result.Tags_), };
}

TThing TThing::Create(const TInitialValues& initial) {
    [[maybe_unused]] auto values = ResolveInitialValues(initial);
    TThing result(TUninitialized{}, std::move(*values.Name), std::move(*values.Count), std::move(*values.Color), std::move(*values.Charges), std::move(*values.Tags));
    return result;
}

TAstNode TThing::GetAst([[maybe_unused]] TAstContext& ctx) const {
    TAstNode node = TAstNode::MakeObject("TThing");
    [[maybe_unused]] const std::string owner_identity = ctx.IdentityOf(this);
    AddValueField(node, "name", Name_);
    AddValueField(node, "count", Count_);
    AddValueField(node, "color", Color_);
    if (!owner_identity.empty() && static_cast<const void*>(&Charges_) != static_cast<const void*>(this)) {
        ctx.RegisterIdentity(&Charges_, owner_identity + ".charges");
    }
    AddOwnedObject(node, "charges", Charges_, ctx);
    {
        TAstNode container = TAstNode::MakeObject("container");
        for (auto value : Tags_) {
            AddValueField(container, ToString(value), value);
        }
        node.AddChild("tags", std::move(container));
    }
    return node;
}

void RegisterBasicAbilities(TAbilityCatalog& catalog) {
    (void)catalog;
}

void RegisterBasicRules(TAbilityRules& rules) {
    RegisterBasicAbilities(rules.Catalog);
}


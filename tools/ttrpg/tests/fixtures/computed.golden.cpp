// AUTO-GENERATED FROM computed.ttrpg -- DO NOT EDIT.
#include <computed.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/rule_integer.h>
#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/dsl/property_registry.h>
#include <pf2e_engine/dsl/value.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <cstdint>
#include <utility>

namespace {
    int ReadGeneratedRuleInteger(const nlohmann::json& value) {
        if (!value.is_number_integer()) {
            throw std::invalid_argument("rule integer JSON must be an integer");
        }
        if (value.is_number_unsigned() && value.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            throw std::overflow_error("rule integer overflow");
        }
        return TRuleInteger::Checked(value.get<std::int64_t>());
    }
}

TPart::TInitialValues TPart::ResolveInitialValues(const TInitialValues& initial) {
    TPart result;
    [[maybe_unused]] auto& self = result;
    (void)initial;
    result.Bonus_ = initial.Bonus ? *initial.Bonus : int{};
    return TInitialValues{.Bonus = std::move(result.Bonus_), };
}

TPart TPart::Create(const TInitialValues& initial) {
    [[maybe_unused]] auto values = ResolveInitialValues(initial);
    TPart r;
    r.Bonus_ = std::move(*values.Bonus);
    return r;
}

TPart TPart::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory) {
    (void)factory;
    TPart r;
    r.Bonus_ = ::ReadGeneratedRuleInteger(j.at("bonus"));
    return r;
}

TAstNode TPart::GetAst([[maybe_unused]] TAstContext& ctx) const {
    TAstNode node = TAstNode::MakeObject("TPart");
    AddValueField(node, "bonus", Bonus_);
    return node;
}

void TPart::RegisterDslProperties() {
    auto& r = TPropertyRegistry<TPart>::Instance();
    r.Register("bonus", [](const TPart* obj, TEvalContext&) {
        return TDslValue(obj->Bonus());
    });
}

TAbility::TInitialValues TAbility::ResolveInitialValues(const TInitialValues& initial) {
    TAbility result;
    [[maybe_unused]] auto& self = result;
    (void)initial;
    result.Value_ = initial.Value ? *initial.Value : int{};
    return TInitialValues{.Value = std::move(result.Value_), };
}

TAbility TAbility::Create(const TInitialValues& initial) {
    [[maybe_unused]] auto values = ResolveInitialValues(initial);
    TAbility r;
    r.Value_ = std::move(*values.Value);
    return r;
}

TAbility TAbility::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory) {
    (void)factory;
    TAbility r;
    r.Value_ = ::ReadGeneratedRuleInteger(j.at("value"));
    return r;
}

TAstNode TAbility::GetAst([[maybe_unused]] TAstContext& ctx) const {
    TAstNode node = TAstNode::MakeObject("TAbility");
    AddValueField(node, "value", Value_);
    return node;
}

void TAbility::RegisterDslProperties() {
    auto& r = TPropertyRegistry<TAbility>::Instance();
    r.Register("value", [](const TAbility* obj, TEvalContext&) {
        return TDslValue(obj->Value());
    });
    r.Register("modifier", [](const TAbility* obj, TEvalContext&) {
        return TDslValue(obj->Modifier());
    });
}

TStats::TInitialValues TStats::ResolveInitialValues(const TInitialValues& initial) {
    TStats result;
    [[maybe_unused]] auto& self = result;
    (void)initial;
    result.Base_ = initial.Base ? *initial.Base : int{};
    result.Level_ = initial.Level ? *initial.Level : 1;
    result.PerLevel_ = initial.PerLevel ? *initial.PerLevel : int{};
    result.Health_ = initial.Health ? *initial.Health : TBoundedQuantity(TRuleInteger::Add((std::as_const(self)).Base(), TRuleInteger::Multiply((std::as_const(self)).Level(), (std::as_const(self)).PerLevel())));
    result.Total_ = initial.Total ? *initial.Total : TRuleInteger::Add((std::as_const(self)).Base(), (std::as_const(self)).PerLevel());
    result.Part_ = initial.Part ? *initial.Part : ::TPart::Create({});
    result.Boosted_ = initial.Boosted ? *initial.Boosted : TRuleInteger::Add(((std::as_const(self)).Part()).Bonus(), (std::as_const(self)).Base());
    result.Ability_ = initial.Ability ? *initial.Ability : ::TAbility::Create({});
    result.ModBoost_ = initial.ModBoost ? *initial.ModBoost : TRuleInteger::Add(((std::as_const(self)).Ability()).Modifier(), (std::as_const(self)).Base());
    return TInitialValues{.Health = std::move(result.Health_), .Base = std::move(result.Base_), .Level = std::move(result.Level_), .PerLevel = std::move(result.PerLevel_), .Total = std::move(result.Total_), .Part = std::move(result.Part_), .Boosted = std::move(result.Boosted_), .Ability = std::move(result.Ability_), .ModBoost = std::move(result.ModBoost_), };
}

TStats TStats::Create(const TInitialValues& initial) {
    [[maybe_unused]] auto values = ResolveInitialValues(initial);
    TStats r;
    r.Base_ = std::move(*values.Base);
    r.Level_ = std::move(*values.Level);
    r.PerLevel_ = std::move(*values.PerLevel);
    r.Health_ = std::move(*values.Health);
    r.Total_ = std::move(*values.Total);
    r.Part_ = std::move(*values.Part);
    r.Boosted_ = std::move(*values.Boosted);
    r.Ability_ = std::move(*values.Ability);
    r.ModBoost_ = std::move(*values.ModBoost);
    return r;
}

TStats TStats::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory) {
    TStats r;
    r.Base_ = ::ReadGeneratedRuleInteger(j.at("base"));
    r.Level_ = j.contains("level") ? ::ReadGeneratedRuleInteger(j.at("level")) : int{1};
    r.PerLevel_ = ::ReadGeneratedRuleInteger(j.at("per_level"));
    r.Health_ = j.contains("health") ? TBoundedQuantity::FromJson(j.at("health"), factory) : TBoundedQuantity((r.Base_ + (r.Level_ * r.PerLevel_)));
    r.Total_ = j.contains("total") ? ::ReadGeneratedRuleInteger(j.at("total")) : (r.Base_ + r.PerLevel_);
    r.Part_ = (j.at("part")).is_string() ? factory.Create<TPart>(TGameObjectIdManager::Instance().Register((j.at("part")).get<std::string>())) : TPart::FromJson(j.at("part"), factory);
    r.Boosted_ = j.contains("boosted") ? ::ReadGeneratedRuleInteger(j.at("boosted")) : (std::as_const(r.Part_).Bonus() + r.Base_);
    r.Ability_ = (j.at("ability")).is_string() ? factory.Create<TAbility>(TGameObjectIdManager::Instance().Register((j.at("ability")).get<std::string>())) : TAbility::FromJson(j.at("ability"), factory);
    r.ModBoost_ = j.contains("mod_boost") ? ::ReadGeneratedRuleInteger(j.at("mod_boost")) : (std::as_const(r.Ability_).Modifier() + r.Base_);
    return r;
}

TAstNode TStats::GetAst([[maybe_unused]] TAstContext& ctx) const {
    TAstNode node = TAstNode::MakeObject("TStats");
    AddOwnedObject(node, "health", Health_, ctx);
    AddValueField(node, "base", Base_);
    AddValueField(node, "level", Level_);
    AddValueField(node, "per_level", PerLevel_);
    AddValueField(node, "total", Total_);
    AddOwnedObject(node, "part", Part_, ctx);
    AddValueField(node, "boosted", Boosted_);
    AddOwnedObject(node, "ability", Ability_, ctx);
    AddValueField(node, "mod_boost", ModBoost_);
    return node;
}

void TStats::RegisterDslProperties() {
    auto& r = TPropertyRegistry<TStats>::Instance();
    r.Register("base", [](const TStats* obj, TEvalContext&) {
        return TDslValue(obj->Base());
    });
    r.Register("level", [](const TStats* obj, TEvalContext&) {
        return TDslValue(obj->Level());
    });
    r.Register("per_level", [](const TStats* obj, TEvalContext&) {
        return TDslValue(obj->PerLevel());
    });
    r.Register("total", [](const TStats* obj, TEvalContext&) {
        return TDslValue(obj->Total());
    });
    r.Register("boosted", [](const TStats* obj, TEvalContext&) {
        return TDslValue(obj->Boosted());
    });
    r.Register("mod_boost", [](const TStats* obj, TEvalContext&) {
        return TDslValue(obj->ModBoost());
    });
}

void RegisterComputedAbilities(TAbilityCatalog& catalog) {
    (void)catalog;
}

void RegisterComputedRules(TAbilityRules& rules) {
    RegisterComputedAbilities(rules.Catalog);
}


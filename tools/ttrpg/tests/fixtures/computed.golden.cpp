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

TPart TPart::Create(const TInitialValues& initial) {
    TPart r;
    if (initial.Bonus) {
        r.Bonus_ = *initial.Bonus;
    }
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

TAbility TAbility::Create(const TInitialValues& initial) {
    TAbility r;
    if (initial.Value) {
        r.Value_ = *initial.Value;
    }
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

TStats TStats::Create(const TInitialValues& initial) {
    TStats r;
    if (initial.Base) {
        r.Base_ = *initial.Base;
    }
    if (initial.Level) {
        r.Level_ = *initial.Level;
    }
    if (initial.PerLevel) {
        r.PerLevel_ = *initial.PerLevel;
    }
    if (initial.Health) {
        r.Health_ = *initial.Health;
    }
    else {
        r.Health_ = TBoundedQuantity((r.Base_ + (r.Level_ * r.PerLevel_)));
    }
    if (initial.Total) {
        r.Total_ = *initial.Total;
    }
    else {
        r.Total_ = (r.Base_ + r.PerLevel_);
    }
    if (initial.Part) {
        r.Part_ = *initial.Part;
    }
    if (initial.Boosted) {
        r.Boosted_ = *initial.Boosted;
    }
    else {
        r.Boosted_ = (std::as_const(r.Part_).Bonus() + r.Base_);
    }
    if (initial.Ability) {
        r.Ability_ = *initial.Ability;
    }
    if (initial.ModBoost) {
        r.ModBoost_ = *initial.ModBoost;
    }
    else {
        r.ModBoost_ = (std::as_const(r.Ability_).Modifier() + r.Base_);
    }
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


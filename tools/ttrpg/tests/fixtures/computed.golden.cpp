// AUTO-GENERATED FROM computed.ttrpg -- DO NOT EDIT.
#include <computed.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/dsl/property_registry.h>
#include <pf2e_engine/dsl/value.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <utility>

TPart TPart::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory) {
    (void)factory;
    TPart r;
    r.Bonus_ = j.at("bonus").get<int>();
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

TAbility TAbility::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory) {
    (void)factory;
    TAbility r;
    r.Value_ = j.at("value").get<int>();
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

TStats TStats::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory) {
    TStats r;
    r.Base_ = j.at("base").get<int>();
    r.Level_ = j.value("level", int{1});
    r.PerLevel_ = j.at("per_level").get<int>();
    r.Health_ = j.contains("health") ? TBoundedQuantity::FromJson(j.at("health"), factory) : TBoundedQuantity((r.Base_ + (r.Level_ * r.PerLevel_)));
    r.Total_ = j.contains("total") ? j.at("total").get<int>() : (r.Base_ + r.PerLevel_);
    r.Part_ = (j.at("part")).is_string() ? factory.Create<TPart>(TGameObjectIdManager::Instance().Register((j.at("part")).get<std::string>())) : TPart::FromJson(j.at("part"), factory);
    r.Boosted_ = j.contains("boosted") ? j.at("boosted").get<int>() : (std::as_const(r.Part_).Bonus() + r.Base_);
    r.Ability_ = (j.at("ability")).is_string() ? factory.Create<TAbility>(TGameObjectIdManager::Instance().Register((j.at("ability")).get<std::string>())) : TAbility::FromJson(j.at("ability"), factory);
    r.ModBoost_ = j.contains("mod_boost") ? j.at("mod_boost").get<int>() : (std::as_const(r.Ability_).Modifier() + r.Base_);
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


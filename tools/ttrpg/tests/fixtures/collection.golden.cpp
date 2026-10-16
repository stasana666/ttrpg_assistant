// AUTO-GENERATED FROM collection.ttrpg -- DO NOT EDIT.
#include <collection.h>

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

TThing::TInitialValues TThing::ResolveInitialValues(const TInitialValues& initial) {
    TThing result;
    [[maybe_unused]] auto& self = result;
    (void)initial;
    result.X_ = initial.X ? *initial.X : int{};
    return TInitialValues{.X = std::move(result.X_), };
}

TThing TThing::Create(const TInitialValues& initial) {
    [[maybe_unused]] auto values = ResolveInitialValues(initial);
    TThing r;
    r.X_ = std::move(*values.X);
    return r;
}

TThing TThing::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory) {
    (void)factory;
    TThing r;
    r.X_ = ::ReadGeneratedRuleInteger(j.at("x"));
    return r;
}

TAstNode TThing::GetAst([[maybe_unused]] TAstContext& ctx) const {
    TAstNode node = TAstNode::MakeObject("TThing");
    AddValueField(node, "x", X_);
    return node;
}

void TThing::RegisterDslProperties() {
    auto& r = TPropertyRegistry<TThing>::Instance();
    r.Register("x", [](const TThing* obj, TEvalContext&) {
        return TDslValue(obj->X());
    });
}

THolder::TInitialValues THolder::ResolveInitialValues(const TInitialValues& initial) {
    THolder result;
    [[maybe_unused]] auto& self = result;
    (void)initial;
    result.Count_ = initial.Count ? *initial.Count : 0;
    result.Things_ = initial.Things ? *initial.Things : TIdCollection<TThing>{};
    return TInitialValues{.Count = std::move(result.Count_), .Things = std::move(result.Things_), };
}

THolder THolder::Create(const TInitialValues& initial) {
    [[maybe_unused]] auto values = ResolveInitialValues(initial);
    THolder r;
    r.Count_ = std::move(*values.Count);
    r.Things_ = std::move(*values.Things);
    return r;
}

THolder THolder::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory) {
    THolder r;
    r.Count_ = j.contains("count") ? ::ReadGeneratedRuleInteger(j.at("count")) : int{0};
    if (j.contains("things")) {
        for (const auto& item : j.at("things")) {
            r.Things_.Add((item).is_string() ? factory.Create<TThing>(TGameObjectIdManager::Instance().Register((item).get<std::string>())) : TThing::FromJson(item, factory));
        }
    }
    return r;
}

TAstNode THolder::GetAst([[maybe_unused]] TAstContext& ctx) const {
    TAstNode node = TAstNode::MakeObject("THolder");
    AddValueField(node, "count", Count_);
    {
        TAstNode coll_node = TAstNode::MakeObject("container");
        AddValueField(coll_node, "next_id", Things_.NextId().Value);
        for (auto entry : Things_) {
            AddOwnedObject(coll_node, std::to_string(entry.Id().Value), *entry, ctx);
        }
        node.AddChild("things", std::move(coll_node));
    }
    return node;
}

void THolder::RegisterDslProperties() {
    auto& r = TPropertyRegistry<THolder>::Instance();
    r.Register("count", [](const THolder* obj, TEvalContext&) {
        return TDslValue(obj->Count());
    });
}

void RegisterCollectionAbilities(TAbilityCatalog& catalog) {
    (void)catalog;
}

void RegisterCollectionRules(TAbilityRules& rules) {
    RegisterCollectionAbilities(rules.Catalog);
}


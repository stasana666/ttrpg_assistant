// AUTO-GENERATED FROM collection.ttrpg -- DO NOT EDIT.
#include <collection.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/dsl/property_registry.h>
#include <pf2e_engine/dsl/value.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <utility>

TThing TThing::Create(const TInitialValues& initial) {
    TThing r;
    if (initial.X) {
        r.X_ = *initial.X;
    }
    return r;
}

TThing TThing::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory) {
    (void)factory;
    TThing r;
    r.X_ = j.at("x").get<int>();
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

THolder THolder::Create(const TInitialValues& initial) {
    THolder r;
    if (initial.Count) {
        r.Count_ = *initial.Count;
    }
    if (initial.Things) {
        r.Things_ = *initial.Things;
    }
    return r;
}

THolder THolder::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory) {
    THolder r;
    r.Count_ = j.value("count", int{0});
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


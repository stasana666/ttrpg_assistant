// AUTO-GENERATED FROM basic.ttrpg -- DO NOT EDIT.
#include <basic.h>

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

TThing TThing::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory) {
    TThing r;
    r.Name_ = j.at("name").get<std::string>();
    r.Count_ = j.value("count", int{3});
    r.Color_ = EColorFromString(j.at("color").get<std::string>());
    r.Charges_ = TBoundedQuantity::FromJson(j.at("charges"), factory);
    if (j.contains("tags")) {
        for (const auto& item : j.at("tags")) {
            r.Tags_.insert(EColorFromString(item.get<std::string>()));
        }
    }
    return r;
}

TAstNode TThing::GetAst([[maybe_unused]] TAstContext& ctx) const {
    TAstNode node = TAstNode::MakeObject("TThing");
    AddValueField(node, "name", Name_);
    AddValueField(node, "count", Count_);
    AddValueField(node, "color", Color_);
    AddOwnedObject(node, "charges", Charges_, ctx);
    {
        TAstNode set_node = TAstNode::MakeObject("container");
        for (auto v : Tags_) {
            AddValueField(set_node, ToString(v), v);
        }
        node.AddChild("tags", std::move(set_node));
    }
    return node;
}

void TThing::RegisterDslProperties() {
    auto& r = TPropertyRegistry<TThing>::Instance();
    // dsl: 'name' skipped -- unsupported primitive 'string'
    r.Register("count", [](const TThing* obj, TEvalContext&) {
        return TDslValue(obj->Count());
    });
    // dsl: 'color' skipped -- enum field 'EColor'
    // dsl: 'charges' skipped -- bounded-quantity field 'BoundedQuantity'
    // dsl: 'tags' skipped -- set field 'EColor'
}


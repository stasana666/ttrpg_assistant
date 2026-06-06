#include <pf2e_engine/common/bounded_quantity.h>

#include <pf2e_engine/common/ast/ast_helpers.h>

#include <nlohmann/json.hpp>

TBoundedQuantity::TBoundedQuantity(int current, int max)
    : current_value_(current)
    , max_value_(max)
{
}

TBoundedQuantity::TBoundedQuantity(int value)
    : current_value_(value)
    , max_value_(value)
{
}

TBoundedQuantity TBoundedQuantity::FromJson(const nlohmann::json& j,
                                            const TGameObjectFactory& factory)
{
    (void)factory;
    return TBoundedQuantity(j.at("current_value").get<int>(), j.at("max_value").get<int>());
}

TAstNode TBoundedQuantity::GetAst([[maybe_unused]] TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TBoundedQuantity");
    AddValueField(node, "current_value", current_value_);
    AddValueField(node, "max_value", max_value_);
    return node;
}

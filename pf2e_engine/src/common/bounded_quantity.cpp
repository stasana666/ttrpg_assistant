#include <pf2e_engine/common/bounded_quantity.h>

#include <pf2e_engine/common/ast/ast_helpers.h>

#include <nlohmann/json.hpp>

#include <stdexcept>

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

void TBoundedQuantity::Reduce(int value)
{
    if (value < 0) {
        throw std::logic_error("negative reduce value");
    }
    current_value_ -= value;
    if (current_value_ < 0) {
        current_value_ = 0;
    }
}

void TBoundedQuantity::Restore(int value)
{
    if (value < 0) {
        throw std::logic_error("negative restore value");
    }
    current_value_ += value;
    if (current_value_ > max_value_) {
        current_value_ = max_value_;
    }
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

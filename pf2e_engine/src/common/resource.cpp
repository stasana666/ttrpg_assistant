#include <pf2e_engine/common/resource.h>

#include <pf2e_engine/common/ast/ast_helpers.h>

#include <nlohmann/json.hpp>

#include <stdexcept>

TResource::TResource(int count)
    : count_(count)
{
}

void TResource::Reduce(int count)
{
    if (count < 0) {
        throw std::logic_error("negative reduce value");
    }
    count_ -= count;
    if (count_ < 0) {
        count_ = 0;
    }
}

void TResource::Add(int count)
{
    if (count < 0) {
        throw std::logic_error("negative add value");
    }
    count_ += count;
}

TResource TResource::FromJson(const nlohmann::json& j, const TGameObjectFactory& factory)
{
    (void)factory;
    return TResource(j.get<int>());
}

TAstNode TResource::GetAst([[maybe_unused]] TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TResource");
    AddValueField(node, "count", count_);
    return node;
}

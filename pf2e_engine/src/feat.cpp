#include <pf2e_engine/feat.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

TAstNode TCreatureFeat::GetAst([[maybe_unused]] TAstContext& ctx) const
{
    static constexpr size_t kExpectedSize = 80;
    AST_ASSERT_LAYOUT(TCreatureFeat, kExpectedSize);

    TAstNode node = TAstNode::MakeObject("TCreatureFeat");
    AddValueField(node, "name", name);
    AddValueField(node, "blocks", blocks);

    TAstNode pipeline_node = TAstNode::MakeObject("pipeline");
    AddValueField(pipeline_node, "count", pipeline.size());
    for (size_t i = 0; i < pipeline.size(); ++i) {
        AddValueField(pipeline_node, std::to_string(i), pipeline[i] != nullptr);
    }
    node.AddChild("pipeline", std::move(pipeline_node));
    return node;
}

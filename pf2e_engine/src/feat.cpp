#include <pf2e_engine/feat.h>

#include <pf2e_engine/common/ast/ast_helpers.h>

TAstNode TCreatureFeat::GetAst([[maybe_unused]] TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TCreatureFeat");
    AddValueField(node, "name", name);
    AddValueField(node, "blocks", blocks);
    // pipeline holds unique_ptrs to IActionBlock (code, not state). Emit only
    // the size and per-slot presence; trying to compare opaque callable trees
    // is fragile and adds nothing for save/rollback validation, which only
    // mutates creature state, not feat definitions.
    TAstNode pipeline_node = TAstNode::MakeObject("pipeline");
    AddValueField(pipeline_node, "count", pipeline.size());
    for (size_t i = 0; i < pipeline.size(); ++i) {
        AddValueField(pipeline_node, std::to_string(i),
            pipeline[i] != nullptr);
    }
    node.AddChild("pipeline", std::move(pipeline_node));
    return node;
}

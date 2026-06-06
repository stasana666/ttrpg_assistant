#pragma once

#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/common/ast/ast_constructable.h>

#include <string>
#include <vector>

struct TCreatureFeat {
    std::string name;
    std::vector<std::string> blocks;
    TAction::TPipeline pipeline;

    TAstNode GetAst(TAstContext& ctx) const;
};

template <>
struct TIsAstRecursive<TCreatureFeat> : std::true_type {};

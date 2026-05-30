#pragma once

#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/common/ast/ast_constructable.h>

#include <string>
#include <vector>

// A persistent creature feat / passive ability. Carries its own block pipeline
// (same JSON block format as actions); the pipeline runs every time an action
// block whose GetName() appears in `blocks` executes on $self, before that
// block's own Apply. Feats may e.g. contribute into $damage_bonus.
//
// `blocks` is always normalised to a list; the JSON accepts either a bare
// string (single hook) or an array of strings (e.g. damage + crit_damage).
//
// Distinct from TTrait<E> in traits_haver.h, which models weapon
// descriptors/tags.
struct TCreatureFeat {
    std::string name;
    std::vector<std::string> blocks;
    TAction::TPipeline pipeline;

    TAstNode GetAst(TAstContext& ctx) const;
};

template <>
struct TIsAstRecursive<TCreatureFeat> : std::true_type {};

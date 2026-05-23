#pragma once

#include <pf2e_engine/actions/action.h>

#include <string>

// A persistent creature feat / passive ability. Carries its own block pipeline
// (same JSON block format as actions); the pipeline runs every time an action
// block whose GetName() matches `block` executes on $self, before that block's
// own Apply. Feats may e.g. contribute into $damage_bonus.
//
// Distinct from TTrait<E> in traits_haver.h, which models weapon
// descriptors/tags.
struct TCreatureFeat {
    std::string name;
    std::string block;
    TAction::TPipeline pipeline;
};

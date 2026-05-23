#pragma once

#include <pf2e_engine/actions/action.h>

#include <string>

// A persistent creature feat / passive ability. Carries its own block pipeline
// (same JSON block format as actions); the pipeline is evaluated during action
// execution and may contribute runtime modifiers (e.g. $damage_bonus).
//
// Distinct from TTrait<E> in traits_haver.h, which models weapon
// descriptors/tags.
struct TCreatureFeat {
    std::string name;
    TAction::TPipeline pipeline;
};

#pragma once

#include <pf2e_engine/random.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

class TGameContext {
public:
    TGameObjectRegistry* game_object_registry;
    IRandomGenerator* dice_roller;
};

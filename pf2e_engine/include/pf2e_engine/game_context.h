#pragma once

#include <pf2e_engine/random.h>
#include <pf2e_engine/game_object_logic/game_object_storage.h>

class TGameContext {
public:
    TGameObjectStorage* gameObjectStorage;
    IRandomGenerator* diceRoller;
};

#pragma once

#include <pf2e_engine/game_context.h>
#include "pf2e_engine/random.h"

inline TGameContext GameContextFrom(IRandomGenerator& rng)
{
    return TGameContext{
        .gameObjectStorage = nullptr,
        .diceRoller = &rng
    };
}

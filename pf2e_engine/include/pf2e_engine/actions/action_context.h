#pragma once

#include <pf2e_engine/random.h>
#include <pf2e_engine/game_object_logic/game_object_register.h>
#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/transformation/transformator.h>

class TActionContext {
public:
    TGameObjectRegister game_object_register;
    TBattleMap* battle_map;
    IRandomGenerator* dice_roller;
    TTransformator* transformator;
};

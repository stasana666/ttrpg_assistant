#pragma once

class IActionBlock;
class TBattle;
class TTransformator;
class IRandomGenerator;
class TGameObjectRegistry;

class TActionContext {
public:
    TGameObjectRegistry* game_object_registry;
    TBattle* battle;
    IRandomGenerator* dice_roller;
    TTransformator* transformator;
    IActionBlock* next_block;
};

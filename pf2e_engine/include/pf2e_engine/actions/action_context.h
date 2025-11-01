#pragma once

class IActionBlock;
class TBattle;
class TTransformator;
class IRandomGenerator;
class TGameObjectRegistry;
class TInteractionSystem;

struct TActionContext {
    TBattle* battle;
    IRandomGenerator* dice_roller;
    TTransformator* transformator;
    TInteractionSystem* io_system;
    IActionBlock* next_block = nullptr;
    TGameObjectRegistry* game_object_registry = nullptr;
};

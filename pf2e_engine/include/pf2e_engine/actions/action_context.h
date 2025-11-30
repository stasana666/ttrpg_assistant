#pragma once

#include <memory>

class IActionBlock;
class TBattle;
class TTransformator;
class IRandomGenerator;
class TGameObjectRegistry;
class TInteractionSystem;
class TTaskScheduler;
class TEffectManager;

struct TActionContext {
    TBattle* battle;
    IRandomGenerator* dice_roller;
    TTransformator* transformator;
    TInteractionSystem* io_system;
    TTaskScheduler* scheduler;
    TEffectManager* effect_manager;
    IActionBlock* next_block = nullptr;
    std::shared_ptr<TGameObjectRegistry> game_object_registry = nullptr;
};

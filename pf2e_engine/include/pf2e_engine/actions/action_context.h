#pragma once

#include <pf2e_engine/common/ability.h>

#include <functional>
#include <memory>
#include <vector>

class IActionBlock;
class TBattle;
class TTransformator;
class IRandomGenerator;
class TGameObjectRegistry;
class IInteractionSystem;
class TTaskScheduler;
class TEffectManager;
class TAction;
class TWeapon;
class TPlayer;
class TGeneratedSourceView;
struct TActionContext;

using FActionInputValidator = std::function<void(const std::shared_ptr<TActionContext>&)>;

struct TActionContext {
    TBattle* battle;
    IRandomGenerator* dice_roller;
    TTransformator* transformator;
    IInteractionSystem* io_system;
    TTaskScheduler* scheduler;
    TEffectManager* effect_manager;
    IActionBlock* next_block = nullptr;
    std::shared_ptr<TGameObjectRegistry> game_object_registry = nullptr;
    std::shared_ptr<const TAction> owned_action = {};
    std::shared_ptr<const TWeapon> source_weapon = {};
    std::shared_ptr<const TGeneratedSourceView> generated_source = {};
    TPlayer* trigger_actor = nullptr;
    TAbilitySourcePath ability_source = {};
    std::function<bool()> validate_ability = {};
    std::shared_ptr<const std::vector<FActionInputValidator>> input_validators = {};
};

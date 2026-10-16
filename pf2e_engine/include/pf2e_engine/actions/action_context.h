#pragma once

#include <pf2e_engine/common/ability.h>
#include <pf2e_engine/actions/action_history.h>

#include <functional>
#include <memory>
#include <vector>
#include <string>

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

enum class EActionExecutionMode {
    Ordinary,
    System,
};

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
    TAbilitySourcePath ability_source = {};
    std::function<bool()> validate_ability = {};
    std::shared_ptr<const std::vector<FActionInputValidator>> input_validators = {};
    std::shared_ptr<const void> owned_reaction = {};
    std::shared_ptr<const std::vector<std::string>> reaction_trace = {};
    EActionExecutionMode execution_mode = EActionExecutionMode::Ordinary;
    std::shared_ptr<const void> owned_lifecycle = {};
    std::optional<TActionRecordStamp> previous_record = {};
    std::shared_ptr<const IActionRecordInput> prepared_record = {};
    TActionTicket action_ticket = {};
    EActionHistoryKind history_kind = EActionHistoryKind::Action;
    std::string action_definition = {};
};

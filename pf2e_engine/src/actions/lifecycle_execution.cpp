#include <pf2e_engine/actions/lifecycle_execution.h>

#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

#include <optional>
#include <stdexcept>
#include <utility>

namespace {

void ValidatePhase(ELifecyclePhase phase)
{
    if (phase != ELifecyclePhase::RoundStart && phase != ELifecyclePhase::TurnStart &&
        phase != ELifecyclePhase::TurnEnd && phase != ELifecyclePhase::TurnStartExpiration) {
        throw std::invalid_argument("unknown lifecycle phase");
    }
}

void ValidateExecution(const std::shared_ptr<const TLifecycleExecution>& execution)
{
    if (!execution || execution->OwnerType == typeid(void) || !execution->Run) {
        throw std::invalid_argument("lifecycle execution requires an owner and a body");
    }
}

}

void TLifecycleExecutions::Register(ELifecyclePhase phase,
                                   std::shared_ptr<const TLifecycleExecution> execution)
{
    ValidatePhase(phase);
    ValidateExecution(execution);
    const auto previous = executions_.find(phase);
    if (previous != executions_.end()) {
        if (previous->second != execution) {
            throw std::invalid_argument("conflicting lifecycle execution");
        }
        return;
    }
    executions_.emplace(phase, std::move(execution));
}

std::shared_ptr<const TLifecycleExecution> TLifecycleExecutions::Find(ELifecyclePhase phase) const
{
    ValidatePhase(phase);
    const auto found = executions_.find(phase);
    return found == executions_.end() ? nullptr : found->second;
}

void InvokeLifecycle(const std::shared_ptr<TActionContext>& services, TPlayer& actor,
                     std::shared_ptr<const TLifecycleExecution> execution,
                     std::function<void()> on_resume)
{
    ValidateExecution(execution);
    if (!services || services->battle == nullptr || services->transformator == nullptr || !on_resume) {
        throw std::invalid_argument("lifecycle execution requires a battle, journal and continuation");
    }
    auto context = std::make_shared<TActionContext>(TActionContext{
        .battle = services->battle,
        .dice_roller = services->dice_roller,
        .transformator = services->transformator,
        .io_system = services->io_system,
        .scheduler = services->scheduler,
        .effect_manager = services->effect_manager,
    });
    context->execution_mode = EActionExecutionMode::System;
    context->owned_lifecycle = execution;
    context->game_object_registry = std::make_shared<TGameObjectRegistry>();
    context->game_object_registry->Add(TGameObjectIdManager::Instance().Register("self"), &actor);
    const auto binding = GeneratedActionSelf(context);
    if (binding.Type() != execution->OwnerType) {
        throw std::invalid_argument("lifecycle actor has a different type");
    }
    enum class ECallMode { Calling, Suspended, Completed, Failed };
    const auto mode = std::make_shared<ECallMode>(ECallMode::Calling);
    auto complete = [context, binding, mode, on_resume = std::move(on_resume)] {
        if (*mode != ECallMode::Suspended) {
            throw std::logic_error("lifecycle continuation requires a suspended body");
        }
        ValidateActionActorBinding(context, binding);
        on_resume();
    };
    try {
        execution->Run(context, std::move(complete));
        *mode = ECallMode::Completed;
        ValidateActionActorBinding(context, binding);
    } catch (const TSavepointStackUnwind&) {
        *mode = ECallMode::Suspended;
        throw;
    } catch (...) {
        *mode = ECallMode::Failed;
        throw;
    }
}

void RunLifecycleAndDrain(const std::shared_ptr<TActionContext>& services, TPlayer& actor,
                          std::shared_ptr<const TLifecycleExecution> execution)
{
    if (!services || services->transformator == nullptr) {
        throw std::invalid_argument("lifecycle execution requires a journal");
    }
    auto& journal = *services->transformator;
    std::optional<TSavepointStackUnwind> pending;
    do {
        try {
            if (pending) {
                pending->Revert(journal);
                pending->Resume();
                pending.reset();
            } else {
                InvokeLifecycle(services, actor, execution, [] {});
            }
        } catch (const TSavepointStackUnwind& next) {
            pending = next;
        }
    } while (pending);
}

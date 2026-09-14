#pragma once

#include <pf2e_engine/actions/action_context.h>

#include <functional>
#include <map>
#include <memory>
#include <typeindex>

enum class ELifecyclePhase {
    RoundStart,
    TurnStart,
    TurnEnd,
    TurnStartExpiration,
};

struct TLifecycleExecution {
    using FRun = std::function<void(std::shared_ptr<TActionContext>, std::function<void()>)>;

    std::type_index OwnerType = typeid(void);
    FRun Run;
};

class TLifecycleExecutions {
public:
    void Register(ELifecyclePhase phase, std::shared_ptr<const TLifecycleExecution> execution);
    std::shared_ptr<const TLifecycleExecution> Find(ELifecyclePhase phase) const;

private:
    std::map<ELifecyclePhase, std::shared_ptr<const TLifecycleExecution>> executions_;
};

void InvokeLifecycle(const std::shared_ptr<TActionContext>& services, TPlayer& actor,
                     std::shared_ptr<const TLifecycleExecution> execution,
                     std::function<void()> on_resume);
void RunLifecycleAndDrain(const std::shared_ptr<TActionContext>& services, TPlayer& actor,
                          std::shared_ptr<const TLifecycleExecution> execution);

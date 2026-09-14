#pragma once

#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/resource_payment.h>
#include <pf2e_engine/common/ability.h>

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <utility>

class TGeneratedWorldView;

template <class TTrigger>
struct TReactionExecution {
    using TValue = typename TTrigger::TState;
    using TArguments = typename TTrigger::TArguments;
    using FResume = std::function<void(TValue)>;
    using FRequirement = std::function<bool(
        const TGeneratedWorldView&, const TPlayer&, const TAbilitySourcePath&,
        const TValue&, const TArguments&)>;
    using FPrepare = std::function<void(
        std::shared_ptr<TActionContext>, TPlayer&, const TGrantedAbility&)>;
    using FRun = std::function<TValue(
        std::shared_ptr<TActionContext>, TValue, TArguments, FResume)>;

    std::type_index OwnerType = typeid(void);
    std::string Name;
    std::string LimitKey;
    TResourcePayment Payment{};
    FRequirement Requirement;
    FPrepare Prepare;
    FRun Run;
};

class TReactionRegistry {
public:
    template <class TTrigger>
    void Register(std::string definition_id,
                  std::shared_ptr<const TReactionExecution<TTrigger>> execution)
    {
        const std::string trigger_id(TTrigger::kId);
        if (definition_id.empty() || trigger_id.empty() || !execution || !execution->Run ||
            execution->Name.empty() || execution->OwnerType == typeid(void)) {
            throw std::invalid_argument("typed reaction requires identifiers, an owner and a body");
        }
        const auto trigger = triggers_.find(trigger_id);
        if (trigger == triggers_.end()) {
            TTriggerExecutions entry{typeid(TTrigger), {}};
            entry.Executions.emplace(std::move(definition_id), std::move(execution));
            triggers_.emplace(trigger_id, std::move(entry));
            return;
        }
        if (trigger->second.Type != typeid(TTrigger)) {
            throw std::invalid_argument("reaction trigger identifier has a different type");
        }
        const auto previous = trigger->second.Executions.find(definition_id);
        if (previous != trigger->second.Executions.end()) {
            if (previous->second.get() != execution.get()) {
                throw std::invalid_argument("conflicting typed reaction execution");
            }
            return;
        }
        trigger->second.Executions.emplace(std::move(definition_id), std::move(execution));
    }

    template <class TTrigger>
    std::shared_ptr<const TReactionExecution<TTrigger>> Find(std::string_view definition_id) const
    {
        const auto trigger = triggers_.find(TTrigger::kId);
        if (trigger == triggers_.end()) {
            return {};
        }
        if (trigger->second.Type != typeid(TTrigger)) {
            throw std::invalid_argument("reaction trigger identifier has a different type");
        }
        const auto execution = trigger->second.Executions.find(definition_id);
        if (execution == trigger->second.Executions.end()) {
            return {};
        }
        return std::static_pointer_cast<const TReactionExecution<TTrigger>>(execution->second);
    }

    bool Contains(std::string_view definition_id) const
    {
        for (const auto& [id, trigger] : triggers_) {
            if (trigger.Executions.contains(definition_id)) {
                return true;
            }
        }
        return false;
    }

private:
    struct TTriggerExecutions {
        std::type_index Type;
        std::map<std::string, std::shared_ptr<const void>, std::less<>> Executions;
    };

    std::map<std::string, TTriggerExecutions, std::less<>> triggers_;
};

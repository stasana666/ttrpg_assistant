#pragma once

#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/resource_payment.h>
#include <pf2e_engine/common/ability.h>
#include <pf2e_engine/common/tagged_registry.h>

#include <functional>
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
        if (!execution || !execution->Run || execution->Name.empty() || execution->OwnerType == typeid(void)) {
            throw std::invalid_argument("typed reaction requires identifiers, an owner and a body");
        }
        registry_.Register<TTrigger>(std::move(definition_id), std::move(execution));
    }

    template <class TTrigger>
    std::shared_ptr<const TReactionExecution<TTrigger>> Find(std::string_view definition_id) const
    {
        return registry_.Find<TTrigger, TReactionExecution<TTrigger>>(definition_id);
    }

    bool Contains(std::string_view definition_id) const { return registry_.Contains(definition_id); }

private:
    TTaggedRegistry registry_;
};

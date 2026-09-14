#pragma once

#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/common/ability.h>
#include <pf2e_engine/common/feature_event.h>

#include <functional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

template <class TResult>
struct TFeatureEventContribution {
    TResult Value;
    std::string FeatureId;
    TAbilitySourcePath Source;
    std::vector<std::string> Provenance;
    TEntityId OwnerId;
};

class TFeatureEventDispatchScope {
public:
    TFeatureEventDispatchScope();
    TFeatureEventDispatchScope(const TFeatureEventDispatchScope&) = delete;
    TFeatureEventDispatchScope& operator=(const TFeatureEventDispatchScope&) = delete;
    ~TFeatureEventDispatchScope();
};

namespace feature_event_detail {

template <class T>
T ReadOnlyArgument(const T& value)
{
    return value;
}

template <class T>
TGeneratedEntityBinding<T> ReadOnlyArgument(const TGeneratedEntityBinding<T>& value)
{
    return value.ReadOnly();
}

template <class T>
void ValidateArgument([[maybe_unused]] const T& value)
{
}

template <class T>
void ValidateArgument(const TGeneratedEntityBinding<T>& value)
{
    value.View().Validate();
}

template <class T>
void ValidateContextArgument([[maybe_unused]] const std::shared_ptr<TActionContext>& context,
                             [[maybe_unused]] const T& value)
{
}

template <class T>
void ValidateContextArgument(const std::shared_ptr<TActionContext>& context,
                             const TGeneratedEntityBinding<T>& value)
{
    ValidateActionEntity(context, value.View());
}

}

template <class TEvent>
std::vector<TFeatureEventContribution<typename TEvent::TResult>> DispatchFeatureEvent(
    const TAbilityRules& rules,
    const TGeneratedEntityBinding<typename TEvent::TOwner>& owner,
    const typename TEvent::TArguments& arguments)
{
    TFeatureEventDispatchScope scope;
    const auto readonly_owner = owner.ReadOnly();
    const auto readonly_arguments = std::apply([](const auto&... argument) {
        return typename TEvent::TArguments{feature_event_detail::ReadOnlyArgument(argument)...};
    }, arguments);
    const auto owner_id = readonly_owner.Read().Id();
    const auto abilities = CollectAbilities(readonly_owner.Read(), rules.Catalog);
    std::vector<TFeatureEventContribution<typename TEvent::TResult>> results;
    for (const auto& feature : abilities.Features()) {
        const auto handler = rules.Events.template Find<TEvent>(feature.DefinitionId);
        if (!handler) {
            continue;
        }
        readonly_owner.View().Validate();
        std::apply([](const auto&... argument) {
            (feature_event_detail::ValidateArgument(argument), ...);
        }, readonly_arguments);
        auto value = [&]() {
            try {
                return std::invoke(*handler, readonly_owner, readonly_arguments);
            } catch (const TSavepointStackUnwind&) {
                throw std::logic_error("pure feature event handlers cannot suspend");
            }
        }();
        results.push_back({std::move(value), feature.DefinitionId, feature.Source,
                           feature.Provenance, owner_id});
    }
    return results;
}

template <class TEvent>
std::vector<TFeatureEventContribution<typename TEvent::TResult>> DispatchFeatureEvent(
    const std::shared_ptr<TActionContext>& context,
    const TGeneratedEntityBinding<typename TEvent::TOwner>& owner,
    const typename TEvent::TArguments& arguments)
{
    const auto& rules = GeneratedActionRules(context);
    ValidateActionEntity(context, owner.View());
    std::apply([&](const auto&... argument) {
        (feature_event_detail::ValidateContextArgument(context, argument), ...);
    }, arguments);
    return DispatchFeatureEvent<TEvent>(rules, owner, arguments);
}

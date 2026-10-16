#pragma once

#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/generated_world.h>
#include <pf2e_engine/actions/generated_history.h>
#include <pf2e_engine/actions/save_point.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

class TReactionScope {
public:
    bool Used(const TGeneratedEntityView& actor, std::string_view key) const;
    TReactionScope WithUse(const TGeneratedEntityView& actor, std::string key) const;

private:
    struct TUse {
        TGeneratedEntityView Actor;
        const TPlayer* Identity;
        std::string Key;
    };

    std::vector<TUse> uses_;
};

template <class TValue>
struct TReactionWindowResult {
    TValue Value;
    TReactionScope Scope;
};

namespace reaction_window_detail {

struct TEnvironment {
    std::shared_ptr<const TActionContext> Services;
    const TAbilityRules* Rules;
    std::vector<TGeneratedEntityView> Participants;
};

std::shared_ptr<const TEnvironment> CreateEnvironment(
    const std::shared_ptr<TActionContext>& parent, std::string_view trigger_id);
std::shared_ptr<TActionContext> ActorContext(const TEnvironment& environment,
                                           const TGeneratedEntityView& actor);
bool ActorAvailable(const TEnvironment& environment, const TGeneratedEntityView& actor);
TAbilityCollector Collect(const TGeneratedEntityView& actor, const TAbilityRules& rules);
void BindExecution(const std::shared_ptr<TActionContext>& context,
                   const TGeneratedEntityView& actor, const TAbilityRules& rules,
                   const TGrantedAbility& grant, std::shared_ptr<const void> execution);

template <class T>
T ReadOnly(const T& value)
{
    return value;
}

template <class T>
TGeneratedEntityBinding<T> ReadOnly(const TGeneratedEntityBinding<T>& value)
{
    return value.ReadOnly();
}

template <class T>
void ValidateArgument([[maybe_unused]] const std::shared_ptr<TActionContext>& context,
                      [[maybe_unused]] const T& value)
{
}

template <class T>
void ValidateArgument(const std::shared_ptr<TActionContext>& context,
                      const TGeneratedEntityBinding<T>& value)
{
    ValidateActionEntityMembership(context, value.View());
}

template <class TArguments>
void ValidateArguments(const std::shared_ptr<TActionContext>& context,
                       const TArguments& arguments)
{
    std::apply([&](const auto&... argument) {
        (ValidateArgument(context, argument), ...);
    }, arguments);
}

template <class TTrigger>
struct TOption {
    TGrantedAbility Grant;
    std::shared_ptr<const TReactionExecution<TTrigger>> Execution;
};

template <class TTrigger>
using TResult = TReactionWindowResult<typename TTrigger::TState>;

template <class TTrigger>
using FResume = std::function<void(TResult<TTrigger>)>;

template <class TTrigger>
bool Eligible(const TEnvironment& environment, const TGeneratedEntityView& actor,
              const TOption<TTrigger>& option, const TResult<TTrigger>& pending,
              const typename TTrigger::TArguments& arguments)
{
    const auto& execution = *option.Execution;
    if (execution.OwnerType != actor.Type() || pending.Scope.Used(actor, execution.LimitKey) ||
        !execution.Payment.Check(actor.Player())) {
        return false;
    }
    if (!execution.Requirement) {
        return true;
    }
    const auto readonly_arguments = std::apply([](const auto&... argument) {
        return typename TTrigger::TArguments{ReadOnly(argument)...};
    }, arguments);
    return execution.Requirement(TGeneratedWorldView(environment.Services->battle),
        actor.Player(), option.Grant.Source, pending.Value, readonly_arguments);
}

template <class TTrigger>
std::vector<TOption<TTrigger>> Options(const TEnvironment& environment,
                                     const TGeneratedEntityView& actor,
                                     const TResult<TTrigger>& pending,
                                     const typename TTrigger::TArguments& arguments)
{
    std::vector<TOption<TTrigger>> result;
    const auto abilities = Collect(actor, *environment.Rules);
    for (const auto& grant : abilities.Grants()) {
        if (RequireGrantImplementation(*environment.Rules, grant) !=
            EAbilityImplementation::TypedReaction) {
            continue;
        }
        auto execution = environment.Rules->Reactions.template Find<TTrigger>(grant.DefinitionId);
        if (execution) {
            TOption<TTrigger> option{grant, std::move(execution)};
            if (Eligible<TTrigger>(environment, actor, option, pending, arguments)) {
                result.push_back(std::move(option));
            }
        }
    }
    return result;
}

template <class TTrigger>
TResult<TTrigger> Execute(const std::shared_ptr<const TEnvironment>& environment,
                         const TGeneratedEntityView& actor, const TOption<TTrigger>& option,
                         TResult<TTrigger> pending, typename TTrigger::TArguments arguments,
                         FResume<TTrigger> on_resume)
{
    auto context = ActorContext(*environment, actor);
    ValidateArguments(context, arguments);
    const auto abilities = Collect(actor, *environment->Rules);
    const auto* current = abilities.FindGrant(option.Grant.DefinitionId, option.Grant.Source);
    if (current == nullptr ||
        RequireGrantImplementation(*environment->Rules, *current) != EAbilityImplementation::TypedReaction ||
        environment->Rules->Reactions.template Find<TTrigger>(current->DefinitionId) != option.Execution ||
        !Eligible<TTrigger>(*environment, actor, option, pending, arguments)) {
        throw TAbilityUnavailable();
    }
    BindExecution(context, actor, *environment->Rules, *current, option.Execution);
    context->input_validators = std::make_shared<const std::vector<FActionInputValidator>>(
        std::vector<FActionInputValidator>{[arguments](const auto& child) {
            ValidateArguments(child, arguments);
        }});
    if (option.Execution->Prepare) {
        option.Execution->Prepare(context, actor.MutablePlayer(), *current);
    }
    ValidateActionActorBinding(context, actor);
    auto next_scope = pending.Scope.WithUse(actor, option.Execution->LimitKey);
    enum class ECallMode { Calling, Suspended, Completed, Failed };
    const auto mode = std::make_shared<ECallMode>(ECallMode::Calling);
    auto complete = [context, actor, next_scope, mode, on_resume = std::move(on_resume)](
                        typename TTrigger::TState value) {
        if (*mode != ECallMode::Suspended) {
            throw std::logic_error("reaction continuation requires a suspended body");
        }
        ValidateActionActorBinding(context, actor);
        on_resume(TResult<TTrigger>{std::move(value), next_scope});
    };
    {
        auto execution_context = context->transformator->ExecutionContext();
        auto transaction = execution_context.Atomic();
        option.Execution->Payment.Consume(*context->transformator, actor.MutablePlayer());
        AcceptActionExecution(context, actor.MutablePlayer(), EActionHistoryKind::Reaction);
        transaction.Commit();
    }
    try {
        auto value = option.Execution->Run(context, std::move(pending.Value),
                                           std::move(arguments), std::move(complete));
        *mode = ECallMode::Completed;
        ValidateActionActorBinding(context, actor);
        return {std::move(value), std::move(next_scope)};
    } catch (const TSavepointStackUnwind&) {
        *mode = ECallMode::Suspended;
        throw;
    } catch (...) {
        *mode = ECallMode::Failed;
        throw;
    }
}

template <class TTrigger>
TResult<TTrigger> Run(const std::shared_ptr<const TEnvironment>& environment,
                     TResult<TTrigger> pending, typename TTrigger::TArguments arguments,
                     std::size_t cursor, FResume<TTrigger> on_resume)
{
    while (cursor < environment->Participants.size()) {
        const auto actor = environment->Participants[cursor++];
        if (!ActorAvailable(*environment, actor)) {
            continue;
        }
        const auto offer = ActorContext(*environment, actor);
        ValidateArguments(offer, arguments);
        const auto options = std::make_shared<const std::vector<TOption<TTrigger>>>(
            Options<TTrigger>(*environment, actor, pending, arguments));
        if (options->empty()) {
            continue;
        }
        auto alternatives = TAlternatives::Create<std::size_t>("reaction");
        alternatives.AddAlternative("Decline", 0);
        std::set<std::string> used_labels{"Decline"};
        for (std::size_t index = 0; index < options->size(); ++index) {
            const auto& name = (*options)[index].Execution->Name;
            auto label = name;
            if (std::ranges::count_if(*options, [&](const auto& option) {
                    return option.Execution->Name == label;
                }) > 1 || label == "Decline") {
                label += " (" + std::to_string(index + 1) + ")";
            }
            std::size_t suffix = index + 1;
            while (!used_labels.insert(label).second) {
                label = name + " (" + std::to_string(++suffix) + ")";
            }
            alternatives.AddAlternative(std::move(label), index + 1);
        }
        auto remainder = [environment, arguments, cursor, on_resume](TResult<TTrigger> next) {
            on_resume(Run<TTrigger>(environment, std::move(next), arguments, cursor, on_resume));
        };
        const auto choice = SelectActionIndex(offer, std::move(alternatives),
            [environment, actor, options, pending, arguments, remainder](std::size_t index) {
                if (index == 0) {
                    remainder(pending);
                } else {
                    remainder(Execute<TTrigger>(environment, actor, options->at(index - 1),
                                                pending, arguments, remainder));
                }
            });
        if (choice != 0) {
            pending = Execute<TTrigger>(environment, actor, options->at(choice - 1),
                                        std::move(pending), arguments, remainder);
        }
    }
    return pending;
}

}

template <class TTrigger>
TReactionWindowResult<typename TTrigger::TState> OpenReactionWindow(
    const std::shared_ptr<TActionContext>& parent, TReactionScope scope,
    typename TTrigger::TState pending, typename TTrigger::TArguments arguments,
    std::function<void(TReactionWindowResult<typename TTrigger::TState>)> on_resume)
{
    if (!on_resume) {
        throw std::invalid_argument("reaction window requires a continuation");
    }
    reaction_window_detail::ValidateArguments(parent, arguments);
    const auto environment = reaction_window_detail::CreateEnvironment(parent, TTrigger::kId);
    return reaction_window_detail::Run<TTrigger>(environment,
        {std::move(pending), std::move(scope)}, std::move(arguments), 0, std::move(on_resume));
}

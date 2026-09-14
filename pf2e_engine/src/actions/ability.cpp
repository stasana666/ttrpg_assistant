#include <pf2e_engine/actions/ability.h>

#include <pf2e_engine/creature.h>
#include <pf2e_engine/player.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

void TAbilityExecutions::Register(std::string id, std::shared_ptr<const TAction> action,
                                  std::optional<ETrigger> trigger,
                                  FAbilityRequirement requirement)
{
    if (id.empty() || !action || trigger == ETrigger::COUNT) {
        throw std::invalid_argument("invalid ability execution binding");
    }
    if (const auto* existing = Find(id)) {
        if (existing->Action != action || existing->Trigger != trigger
            || existing->Requirement || requirement) {
            throw std::invalid_argument("conflicting ability execution: " + id);
        }
        return;
    }
    executions_.emplace(std::move(id), TAbilityExecution{
        .Action = std::move(action),
        .Trigger = trigger,
        .Requirement = std::move(requirement),
    });
}

const TAbilityExecution* TAbilityExecutions::Find(std::string_view id) const
{
    const auto found = executions_.find(id);
    return found == executions_.end() ? nullptr : &found->second;
}

const TAbilityExecution& TAbilityExecutions::Require(std::string_view id) const
{
    const auto* found = Find(id);
    if (found == nullptr) {
        throw std::invalid_argument("missing ability execution: " + std::string(id));
    }
    return *found;
}

TAbilityUnavailable::TAbilityUnavailable()
    : std::runtime_error("ability is no longer available")
{
}

namespace {

bool IsEligible(const TAbilityExecution& execution, const TGrantedAbility& grant,
                const TPlayer& actor, const TTriggerContext* trigger)
{
    if (actor.GetCreature() == nullptr || !actor.GetCreature()->IsAlive()) {
        return false;
    }
    if (grant.Kind == EAbilityKind::Action) {
        if (execution.Trigger.has_value()) {
            throw std::invalid_argument("action execution must not have a reaction trigger");
        }
        if (trigger != nullptr) {
            return false;
        }
    } else if (grant.Kind == EAbilityKind::Reaction) {
        if (!execution.Trigger.has_value()) {
            throw std::invalid_argument("reaction execution must have a trigger");
        }
        if (trigger == nullptr || trigger->triggered_player == nullptr
            || execution.Trigger != trigger->type) {
            return false;
        }
    } else {
        return false;
    }
    if (!execution.Action->Check(actor)) {
        return false;
    }
    return !execution.Requirement || execution.Requirement(actor, grant.Source, trigger);
}

std::vector<TAbilityOption> Enumerate(
    int actor_id, const TAbilityCollector& collector, const TAbilityExecutions& executions,
    const TPlayer& actor, const TTriggerContext* trigger)
{
    if (actor_id != actor.GetId()) {
        throw std::invalid_argument("ability actor id does not match player");
    }
    std::vector<TAbilityOption> options;
    for (const auto& grant : collector.Grants()) {
        const auto kind = trigger == nullptr ? EAbilityKind::Action : EAbilityKind::Reaction;
        if (grant.Kind != kind) {
            continue;
        }
        const auto& execution = executions.Require(grant.DefinitionId);
        if (!IsEligible(execution, grant, actor, trigger)) {
            continue;
        }
        std::optional<TAbilityTriggerBinding> binding;
        if (trigger != nullptr) {
            binding = TAbilityTriggerBinding{trigger->type, trigger->triggered_player->GetId(),
                                             trigger->triggered_player};
        }
        options.push_back(TAbilityOption{
            .ActorId = actor_id,
            .ActorIdentity = &actor,
            .DefinitionId = grant.DefinitionId,
            .Source = grant.Source,
            .Provenance = grant.Provenance,
            .Action = execution.Action,
            .Trigger = binding,
        });
    }
    return options;
}

}

std::vector<TAbilityOption> EnumerateActions(
    int actor_id, const TAbilityCollector& collector, const TAbilityExecutions& executions,
    const TPlayer& actor)
{
    return Enumerate(actor_id, collector, executions, actor, nullptr);
}

std::vector<TAbilityOption> EnumerateReactions(
    int actor_id, const TAbilityCollector& collector, const TAbilityExecutions& executions,
    const TPlayer& actor, const TTriggerContext& trigger)
{
    return Enumerate(actor_id, collector, executions, actor, &trigger);
}

EAbilityExecutionResult ExecuteAbility(
    const TAbilityOption& option, std::shared_ptr<TActionContext> ctx, TPlayer& actor,
    const TAbilityCollector& fresh, const TAbilityExecutions& executions,
    TAbilityExecutionBindings bindings)
{
    if (option.ActorId != actor.GetId() || option.ActorIdentity != &actor) {
        return EAbilityExecutionResult::Unavailable;
    }
    const auto& grants = fresh.Grants();
    const auto grant = std::find_if(grants.begin(), grants.end(), [&](const auto& current) {
        return current.DefinitionId == option.DefinitionId && current.Source == option.Source;
    });
    if (grant == grants.end()) {
        return EAbilityExecutionResult::Unavailable;
    }
    const auto& execution = executions.Require(option.DefinitionId);
    if (execution.Action != option.Action) {
        return EAbilityExecutionResult::Unavailable;
    }
    std::optional<TTriggerContext> trigger;
    if (option.Trigger) {
        if (bindings.TriggerActor == nullptr
            || bindings.TriggerActor->GetId() != option.Trigger->ActorId
            || bindings.TriggerActor != option.Trigger->ActorIdentity) {
            return EAbilityExecutionResult::Unavailable;
        }
        trigger = TTriggerContext{option.Trigger->Type, bindings.TriggerActor};
    }
    if (!IsEligible(execution, *grant, actor, trigger ? &*trigger : nullptr)
        || (bindings.ValidateSource && !bindings.ValidateSource())) {
        return EAbilityExecutionResult::Unavailable;
    }
    ctx->owned_action = execution.Action;
    ctx->source_weapon = std::move(bindings.SourceWeapon);
    ctx->trigger_actor = bindings.TriggerActor;
    ctx->ability_source = option.Source;
    ctx->validate_ability = std::move(bindings.ValidateSource);
    execution.Action->Apply(std::move(ctx), actor);
    return EAbilityExecutionResult::Applied;
}

void ValidateAbilitySource(const std::shared_ptr<TActionContext>& ctx)
{
    if (ctx->validate_ability && !ctx->validate_ability()) {
        throw TAbilityUnavailable();
    }
}

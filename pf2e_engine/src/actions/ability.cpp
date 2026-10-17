#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_world.h>
#include <pf2e_engine/actions/generated_history.h>

#include <pf2e_engine/creature.h>
#include <pf2e_engine/player.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

void TAbilityExecutions::Register(std::string id, std::shared_ptr<const TAction> action)
{
    Register(std::move(id), std::make_shared<const TAbilityExecution>(TAbilityExecution{
        .Action = std::move(action),
        .Requirement = {},
    }));
}

void TAbilityExecutions::Register(std::string id,
                                  std::shared_ptr<const TAbilityExecution> execution)
{
    if (id.empty() || !execution || !execution->Action) {
        throw std::invalid_argument("invalid ability execution binding");
    }
    if (const auto found = executions_.find(id); found != executions_.end()) {
        const auto& existing = found->second;
        if (existing == execution) {
            return;
        }
        if (existing->Action != execution->Action || existing->Requirement || execution->Requirement
            || existing->Prepare || execution->Prepare || existing->PreviousRecord != execution->PreviousRecord) {
            throw std::invalid_argument("conflicting ability execution: " + id);
        }
        return;
    }
    executions_.emplace(std::move(id), std::move(execution));
}

const TAbilityExecution* TAbilityExecutions::Find(std::string_view id) const
{
    const auto found = executions_.find(id);
    return found == executions_.end() ? nullptr : found->second.get();
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

EAbilityImplementation RequireGrantImplementation(const TAbilityRules& rules,
                                                   const TGrantedAbility& grant)
{
    const auto& definition = rules.Catalog.Require(grant.DefinitionId);
    const auto* execution = rules.Executions.Find(grant.DefinitionId);
    const bool typed = rules.Reactions.Contains(grant.DefinitionId);
    if (definition.Kind != grant.Kind || (execution != nullptr && typed)) {
        throw std::invalid_argument("ability has conflicting execution bindings");
    }
    if (grant.Kind == EAbilityKind::Action && execution != nullptr) {
        return EAbilityImplementation::Action;
    }
    if (grant.Kind == EAbilityKind::Reaction && typed) {
        return EAbilityImplementation::TypedReaction;
    }
    throw std::invalid_argument("missing or mismatched ability implementation: " + grant.DefinitionId);
}

namespace {

bool IsEligible(const TAbilityExecution& execution, const TGrantedAbility& grant,
                const TPlayer& actor, const TGeneratedWorldView& world)
{
    if (actor.GetCreature() == nullptr || !actor.GetCreature()->CanAct()) {
        return false;
    }
    if (world.IsBound()) {
        world.Validate(BindGeneratedActionEntity(actor));
    }
    if (grant.Kind != EAbilityKind::Action || !execution.Action->Check(actor)) {
        return false;
    }
    return !execution.Requirement || execution.Requirement(world, actor, grant.Source);
}

}

std::vector<TAbilityOption> EnumerateActions(
    int actor_id, const TAbilityCollector& collector, const TAbilityExecutions& executions,
    const TPlayer& actor, const TGeneratedWorldView* bound_world)
{
    if (actor_id != actor.GetId()) {
        throw std::invalid_argument("ability actor id does not match player");
    }
    const auto world = bound_world != nullptr ? *bound_world : TGeneratedWorldView{};
    std::vector<TAbilityOption> options;
    for (const auto& grant : collector.Grants()) {
        if (grant.Kind != EAbilityKind::Action) {
            continue;
        }
        const auto& execution = executions.Require(grant.DefinitionId);
        if (!IsEligible(execution, grant, actor, world)) {
            continue;
        }
        std::optional<TActionRecordStamp> previous;
        if (!execution.PreviousRecord.empty()) {
            previous = FindActionRecordStamp(world, actor, execution.PreviousRecord);
            if (!previous) {
                continue;
            }
        }
        options.push_back(TAbilityOption{
            .ActorId = actor_id,
            .ActorIdentity = &actor,
            .DefinitionId = grant.DefinitionId,
            .Source = grant.Source,
            .Provenance = grant.Provenance,
            .Action = execution.Action,
            .PreviousRecord = std::move(previous),
        });
    }
    return options;
}

EAbilityExecutionResult ExecuteAbility(
    const TAbilityOption& option, std::shared_ptr<TActionContext> ctx, TPlayer& actor,
    const TAbilityCollector& fresh, const TAbilityExecutions& executions,
    TAbilityExecutionBindings bindings)
{
    if (ctx) {
        ctx->execution_mode = EActionExecutionMode::Ordinary;
        ctx->owned_lifecycle.reset();
    }
    if (option.ActorId != actor.GetId() || option.ActorIdentity != &actor) {
        return EAbilityExecutionResult::Unavailable;
    }
    const auto* grant = fresh.FindGrant(option.DefinitionId, option.Source);
    if (grant == nullptr) {
        return EAbilityExecutionResult::Unavailable;
    }
    const auto& execution = executions.Require(option.DefinitionId);
    if (execution.Action != option.Action) {
        return EAbilityExecutionResult::Unavailable;
    }
    const TGeneratedWorldView world(ctx ? ctx->battle : nullptr);
    if (!execution.PreviousRecord.empty()
        && (!option.PreviousRecord || !MatchActionRecordStamp(world, actor, *option.PreviousRecord,
                                                              execution.PreviousRecord))) {
        return EAbilityExecutionResult::Unavailable;
    }
    if (!IsEligible(execution, *grant, actor, world)
        || (bindings.ValidateSource && !bindings.ValidateSource())) {
        return EAbilityExecutionResult::Unavailable;
    }
    if (!ctx) {
        throw std::invalid_argument("ability execution requires a context");
    }
    ctx->generated_source.reset();
    ctx->input_validators.reset();
    ctx->owned_reaction.reset();
    ctx->reaction_trace.reset();
    ctx->previous_record = option.PreviousRecord;
    ctx->prepared_record.reset();
    ctx->action_ticket = {};
    ctx->history_kind = EActionHistoryKind::Action;
    ctx->action_definition = option.DefinitionId;
    ctx->owned_action = execution.Action;
    ctx->ability_source = option.Source;
    ctx->validate_ability = std::move(bindings.ValidateSource);
    ctx->actor = &actor;
    if (execution.Prepare) {
        execution.Prepare(ctx, actor, *grant);
    }
    execution.Action->Apply(std::move(ctx), actor);
    return EAbilityExecutionResult::Applied;
}

void ValidateAbilitySource(const std::shared_ptr<TActionContext>& ctx)
{
    if (!ctx) {
        throw std::invalid_argument("ability source requires an execution context");
    }
    if (ctx->validate_ability && !ctx->validate_ability()) {
        throw TAbilityUnavailable();
    }
    ValidateActionInputs(ctx);
}

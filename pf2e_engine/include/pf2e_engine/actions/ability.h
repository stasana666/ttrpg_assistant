#pragma once

#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/actions/reaction.h>
#include <pf2e_engine/common/ability.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using FAbilityRequirement = std::function<bool(
    const TPlayer&, const TAbilitySourcePath&, const TTriggerContext*)>;

struct TCreatureFeat;

struct TAbilityExecution {
    std::shared_ptr<const TAction> Action;
    std::optional<ETrigger> Trigger;
    FAbilityRequirement Requirement;
};

class TAbilityExecutions {
public:
    void Register(std::string id, std::shared_ptr<const TAction> action,
                  std::optional<ETrigger> trigger = {}, FAbilityRequirement requirement = {});
    const TAbilityExecution* Find(std::string_view id) const;
    const TAbilityExecution& Require(std::string_view id) const;

private:
    std::map<std::string, TAbilityExecution, std::less<>> executions_;
};

struct TAbilityRules {
    TAbilityCatalog Catalog;
    TAbilityExecutions Executions;
    std::map<std::string, std::shared_ptr<const TCreatureFeat>, std::less<>> FeatureHooks;
};

struct TAbilityTriggerBinding {
    ETrigger Type;
    int ActorId;
    const TPlayer* ActorIdentity = nullptr;

    friend bool operator==(const TAbilityTriggerBinding&, const TAbilityTriggerBinding&) = default;
};

struct TAbilityOption {
    int ActorId;
    const TPlayer* ActorIdentity = nullptr;
    std::string DefinitionId;
    TAbilitySourcePath Source;
    std::vector<std::string> Provenance;
    std::shared_ptr<const TAction> Action;
    std::optional<TAbilityTriggerBinding> Trigger;
};

struct TAbilityExecutionBindings {
    std::shared_ptr<const TWeapon> SourceWeapon = {};
    TPlayer* TriggerActor = nullptr;
    std::function<bool()> ValidateSource = {};
};

enum class EAbilityExecutionResult {
    Applied,
    Unavailable,
};

class TAbilityUnavailable : public std::runtime_error {
public:
    TAbilityUnavailable();
};

std::vector<TAbilityOption> EnumerateActions(
    int actor_id, const TAbilityCollector& collector, const TAbilityExecutions& executions,
    const TPlayer& actor);
std::vector<TAbilityOption> EnumerateReactions(
    int actor_id, const TAbilityCollector& collector, const TAbilityExecutions& executions,
    const TPlayer& actor, const TTriggerContext& trigger);
EAbilityExecutionResult ExecuteAbility(
    const TAbilityOption& option, std::shared_ptr<TActionContext> ctx, TPlayer& actor,
    const TAbilityCollector& fresh, const TAbilityExecutions& executions,
    TAbilityExecutionBindings bindings = {});

void ValidateAbilitySource(const std::shared_ptr<TActionContext>& ctx);

#pragma once

#include <pf2e_engine/actions/timed_relation.h>
#include <pf2e_engine/common/game_time.h>

#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/actions/reaction_execution.h>
#include <pf2e_engine/actions/lifecycle_execution.h>
#include <pf2e_engine/common/ability.h>
#include <pf2e_engine/common/feature_event.h>

#include <functional>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

class TGeneratedWorldView;

using FLegacyAbilityRequirement = std::function<bool(const TPlayer&, const TAbilitySourcePath&)>;
using FAbilityRequirement = std::function<bool(
    const TGeneratedWorldView&, const TPlayer&, const TAbilitySourcePath&)>;
using FAbilityPreparation = std::function<void(
    std::shared_ptr<TActionContext>, TPlayer&, const TGrantedAbility&)>;

struct TCreatureFeat;

struct TAbilityExecution {
    std::shared_ptr<const TAction> Action;
    FAbilityRequirement Requirement;
    FAbilityPreparation Prepare = {};
    std::string PreviousRecord = {};
};

class TAbilityExecutions {
public:
    void Register(std::string id, std::shared_ptr<const TAction> action,
                  FLegacyAbilityRequirement requirement = {});
    void Register(std::string id, std::shared_ptr<const TAbilityExecution> execution);
    const TAbilityExecution* Find(std::string_view id) const;
    const TAbilityExecution& Require(std::string_view id) const;

private:
    std::map<std::string, std::shared_ptr<const TAbilityExecution>, std::less<>> executions_;
};

struct TAbilityRules {
    TAbilityCatalog Catalog;
    TAbilityExecutions Executions;
    TReactionRegistry Reactions;
    TLifecycleExecutions Lifecycle;
    TFeatureEventRegistry Events;
    TActionRecordRegistry Records;
    TRelationRegistry Relations;
    TGameTimeRules Time;
    std::map<std::string, std::shared_ptr<const TCreatureFeat>, std::less<>> FeatureHooks;
};

enum class EAbilityImplementation {
    Action,
    TypedReaction,
};

EAbilityImplementation RequireGrantImplementation(const TAbilityRules& rules,
                                                   const TGrantedAbility& grant);

struct TAbilityOption {
    int ActorId;
    const TPlayer* ActorIdentity = nullptr;
    std::string DefinitionId;
    TAbilitySourcePath Source;
    std::vector<std::string> Provenance;
    std::shared_ptr<const TAction> Action;
    std::optional<TActionRecordStamp> PreviousRecord = {};
};

struct TAbilityExecutionBindings {
    std::shared_ptr<const TWeapon> SourceWeapon = {};
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
    const TPlayer& actor, const TGeneratedWorldView* world = nullptr);
EAbilityExecutionResult ExecuteAbility(
    const TAbilityOption& option, std::shared_ptr<TActionContext> ctx, TPlayer& actor,
    const TAbilityCollector& fresh, const TAbilityExecutions& executions,
    TAbilityExecutionBindings bindings = {});

void ValidateAbilitySource(const std::shared_ptr<TActionContext>& ctx);

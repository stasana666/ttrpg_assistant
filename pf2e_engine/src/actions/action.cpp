#include <action.h>

#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/common/continuation.h>
#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/mechanics/creature.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

#include <limits>
#include <map>
#include <stdexcept>

namespace {

const TGameObjectId kSelf = TGameObjectIdManager::Instance().Register("self");
const TGameObjectId kSource = TGameObjectIdManager::Instance().Register("source");
const TGameObjectId kWeapon = TGameObjectIdManager::Instance().Register("weapon");
const TGameObjectId kTriggerActor = TGameObjectIdManager::Instance().Register("trigger_actor");
const TGameObjectId kTarget = TGameObjectIdManager::Instance().Register("target");

void AddCost(int& total, int count)
{
    if (count < 0 || count > std::numeric_limits<int>::max() - total) {
        throw std::invalid_argument("invalid action resource cost");
    }
    total += count;
}

void ValidateCosts(const TAction::TBoundResources& costs)
{
    std::map<std::string, int, std::less<>> totals;
    for (const auto& cost : costs) {
        if (cost.Key.empty() || !cost.Read || !cost.Write) {
            throw std::invalid_argument("invalid action resource binding");
        }
        AddCost(totals[cost.Key], cost.Count);
    }
}

TAction::TBoundResources BindLegacyCosts(const TAction::TResources& costs)
{
    TAction::TBoundResources bindings;
    for (const auto& cost : costs) {
        if (cost.kind != EResourceKind::Action && cost.kind != EResourceKind::Reaction) {
            throw std::invalid_argument("invalid action resource kind");
        }
        bindings.push_back({
            .Key = cost.kind == EResourceKind::Action ? "self.actions" : "self.reactions",
            .Count = cost.count,
            .Read = [kind = cost.kind](const TPlayer& actor) -> const TResource& {
                return ResourceFor(*actor.GetCreature(), kind);
            },
            .Write = [kind = cost.kind](TPlayer& actor) {
                return ResourceFor(*actor.GetCreature(), kind);
            },
        });
    }
    return bindings;
}

struct TResourceTotal {
    const TResource* Resource;
    std::size_t FirstBinding;
    int Count = 0;
};

struct TResolvedReadCosts {
    std::vector<const TResource*> Bindings;
    std::vector<TResourceTotal> Totals;
};

TResolvedReadCosts ResolveReadCosts(const TAction::TBoundResources& costs, const TPlayer& actor)
{
    TResolvedReadCosts resolved;
    std::map<std::string, const TResource*, std::less<>> keys;
    std::map<const TResource*, std::size_t> resources;
    for (const auto& cost : costs) {
        const auto* resource = &cost.Read(actor);
        const auto [key, key_inserted] = keys.emplace(cost.Key, resource);
        if (!key_inserted && key->second != resource) {
            throw std::invalid_argument("action resource key refers to different resources");
        }
        const auto [entry, inserted] = resources.emplace(resource, resolved.Totals.size());
        if (inserted) {
            resolved.Totals.push_back({resource, resolved.Bindings.size()});
        }
        AddCost(resolved.Totals[entry->second].Count, cost.Count);
        resolved.Bindings.push_back(resource);
    }
    return resolved;
}

void RunValidatedAbility(const std::shared_ptr<TActionContext>& ctx,
                         const TSavepointCallback& callback)
{
    ValidateAbilitySource(ctx);
    try {
        callback();
    } catch (TSavepointStackUnwind& save_point) {
        save_point.AddCallFunctionLevel([ctx](TSavepointCallback resume) {
            RunValidatedAbility(ctx, resume);
        });
        throw;
    }
}

}

TAction::TAction(TPipeline&& pipeline, TResources&& consume, std::string&& name,
                 TVariables&& variables)
    : pipeline_(std::move(pipeline))
    , consume_(BindLegacyCosts(consume))
    , name_(std::move(name))
    , variables_(std::move(variables))
{
    ValidateCosts(consume_);
    for (const auto& variable : variables_) {
        if (variable.id == kSelf || variable.id == kSource || variable.id == kWeapon
            || variable.id == kTriggerActor || variable.id == kTarget || !variable.weapon) {
            throw std::invalid_argument("invalid or reserved action variable");
        }
    }
}

TAction::TAction(FBody body, TBoundResources costs, std::string name)
    : body_(std::move(body))
    , consume_(std::move(costs))
    , name_(std::move(name))
{
    if (!body_) {
        throw std::invalid_argument("generated action body must not be empty");
    }
    ValidateCosts(consume_);
}

void TAction::Apply(std::shared_ptr<TActionContext> ctx, TPlayer& player) const
{
    if (!Check(player)) {
        throw TAbilityUnavailable();
    }
    ValidateAbilitySource(ctx);
    ctx->next_block = pipeline_.empty() ? nullptr : pipeline_.front().get();
    ctx->game_object_registry = std::make_shared<TGameObjectRegistry>();
    ctx->game_object_registry->Add(kSelf, &player);
    if (ctx->source_weapon) {
        ctx->game_object_registry->Add(kSource, ctx->source_weapon.get());
        ctx->game_object_registry->Add(kWeapon, ctx->source_weapon.get());
    }
    if (ctx->trigger_actor != nullptr) {
        ctx->game_object_registry->Add(kTriggerActor, ctx->trigger_actor);
        ctx->game_object_registry->Add(kTarget, ctx->trigger_actor);
    }

    for (const auto& variable : variables_) {
        ctx->game_object_registry->Add(variable.id, TGameObjectPtr{variable.weapon.get()});
    }

    Consume(ctx, player);

    ctx->io_system->GameLog() << player.GetName() << ": " << name_ << std::endl;

    if (body_) {
        RunValidatedAbility(ctx, [this, ctx]() { body_(ctx); });
    } else {
        RunValidatedAbility(ctx, [ctx]() {
            continuation::While(
                [ctx]() { return ctx->next_block != nullptr; },
                [ctx]() { ctx->next_block->Run(ctx); });
        });
    }
}

void TAction::Consume(std::shared_ptr<TActionContext> ctx, TPlayer& player) const
{
    const auto resolved = ResolveReadCosts(consume_, std::as_const(player));
    std::vector<TGuarded<TResource>> targets;
    targets.reserve(consume_.size());
    for (std::size_t index = 0; index < consume_.size(); ++index) {
        auto target = consume_[index].Write(player);
        if (&target.Get() != resolved.Bindings[index]) {
            throw std::invalid_argument("action resource read and write bindings disagree");
        }
        targets.push_back(target);
    }
    for (const auto& total : resolved.Totals) {
        if (!total.Resource->Has(total.Count)) {
            throw TAbilityUnavailable();
        }
    }
    auto execution = ctx->transformator->ExecutionContext();
    auto transaction = execution.Atomic();
    for (const auto& total : resolved.Totals) {
        if (total.Count != 0) {
            ctx->transformator->ReduceResource(targets[total.FirstBinding], total.Count);
        }
    }
    transaction.Commit();
}

bool TAction::Check(const TPlayer& player) const
{
    for (const auto& total : ResolveReadCosts(consume_, player).Totals) {
        if (!total.Resource->Has(total.Count)) {
            return false;
        }
    }
    return true;
}

std::string_view TAction::Name() const
{
    return name_;
}

void RunSubPipeline(std::shared_ptr<TActionContext> ctx, IActionBlock* first)
{
    IActionBlock* saved = ctx->next_block;
    ctx->next_block = first;
    continuation::Then(
        [ctx]() {
            continuation::While(
                [ctx]() { return ctx->next_block != nullptr; },
                [ctx]() { ctx->next_block->Run(ctx); });
        },
        [ctx, saved]() { ctx->next_block = saved; });
}

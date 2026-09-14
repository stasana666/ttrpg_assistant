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

#include <stdexcept>

namespace {

const TGameObjectId kSelf = TGameObjectIdManager::Instance().Register("self");
const TGameObjectId kSource = TGameObjectIdManager::Instance().Register("source");
const TGameObjectId kWeapon = TGameObjectIdManager::Instance().Register("weapon");
const TGameObjectId kTriggerActor = TGameObjectIdManager::Instance().Register("trigger_actor");
const TGameObjectId kTarget = TGameObjectIdManager::Instance().Register("target");

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
    , payment_(BindLegacyCosts(consume))
    , name_(std::move(name))
    , variables_(std::move(variables))
{
    for (const auto& variable : variables_) {
        if (variable.id == kSelf || variable.id == kSource || variable.id == kWeapon
            || variable.id == kTriggerActor || variable.id == kTarget || !variable.weapon) {
            throw std::invalid_argument("invalid or reserved action variable");
        }
    }
}

TAction::TAction(FBody body, TBoundResources costs, std::string name)
    : body_(std::move(body))
    , payment_(std::move(costs))
    , name_(std::move(name))
{
    if (!body_) {
        throw std::invalid_argument("generated action body must not be empty");
    }
}

TAction::TAction(TCompiledBody body, TBoundResources costs, std::string name)
    : TAction(std::move(body.Run), std::move(costs), std::move(name))
{
    compiled_body_ = true;
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

    if (compiled_body_) {
        body_(ctx);
    } else if (body_) {
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
    if (!ctx || ctx->transformator == nullptr) {
        throw std::invalid_argument("resource payment requires a journal");
    }
    payment_.Consume(*ctx->transformator, player);
}

bool TAction::Check(const TPlayer& player) const
{
    return payment_.Check(player);
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

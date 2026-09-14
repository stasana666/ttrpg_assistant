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
    , consume_(std::move(consume))
    , name_(std::move(name))
    , variables_(std::move(variables))
{
    std::map<EResourceKind, int> totals;
    for (const auto& cost : consume_) {
        if ((cost.kind != EResourceKind::Action && cost.kind != EResourceKind::Reaction)
            || cost.count < 0 || cost.count > std::numeric_limits<int>::max() - totals[cost.kind]) {
            throw std::invalid_argument("invalid action resource cost");
        }
        totals[cost.kind] += cost.count;
    }
    consume_.clear();
    for (const auto& [kind, count] : totals) {
        consume_.push_back(TResourceCost{kind, count});
    }
    for (const auto& variable : variables_) {
        if (variable.id == kSelf || variable.id == kSource || variable.id == kWeapon
            || variable.id == kTriggerActor || variable.id == kTarget || !variable.weapon) {
            throw std::invalid_argument("invalid or reserved action variable");
        }
    }
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

    RunValidatedAbility(ctx, [ctx]() {
        continuation::While(
            [ctx]() { return ctx->next_block != nullptr; },
            [ctx]() { ctx->next_block->Run(ctx); });
    });
}

void TAction::Consume(std::shared_ptr<TActionContext> ctx, TPlayer& player) const
{
    if (!Check(player)) {
        throw TAbilityUnavailable();
    }
    for (const auto& resource : consume_) {
        ctx->transformator->ReduceResource(
            ResourceFor(*player.GetCreature(), resource.kind), resource.count);
    }
}

bool TAction::Check(const TPlayer& player) const
{
    for (const auto& resource : consume_) {
        if (!ResourceFor(*player.GetCreature(), resource.kind).Has(resource.count)) {
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

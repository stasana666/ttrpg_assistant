#include <action.h>

#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/common/continuation.h>
#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

const TGameObjectId kSelf = TGameObjectIdManager::Instance().Register("self");

TAction::TAction(TPipeline&& pipeline, TResources&& consume, std::string&& name,
                 TVariables&& variables)
    : pipeline_(std::move(pipeline))
    , consume_(std::move(consume))
    , name_(std::move(name))
    , variables_(std::move(variables))
{
}

void TAction::Apply(std::shared_ptr<TActionContext> ctx, TPlayer& player)
{
    ctx->next_block = pipeline_.begin()->get();
    ctx->game_object_registry = std::make_shared<TGameObjectRegistry>();
    ctx->game_object_registry->Add(kSelf, &player);

    for (const auto& variable : variables_) {
        ctx->game_object_registry->Add(variable.id, TGameObjectPtr{variable.weapon.get()});
    }

    Consume(ctx, player);

    ctx->io_system->GameLog() << player.GetName() << ": " << name_ << std::endl;

    continuation::While(
        [ctx]() { return ctx->next_block != nullptr; },
        [ctx]() { ctx->next_block->Run(ctx); });
}

void TAction::Consume(std::shared_ptr<TActionContext> ctx, TPlayer& player)
{
    for (auto& resource : consume_) {
        ctx->transformator->ReduceResource(
            player.GetCreature()->ResourceFor(resource.kind), resource.count);
    }
}

bool TAction::Check(const TPlayer& player)
{
    for (auto& resource : consume_) {
        if (!player.GetCreature()->ResourceFor(resource.kind).Has(resource.count)) {
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
    // Restoring next_block must survive suspension: if the sub-pipeline throws a
    // savepoint, the restore has to run when it resumes, not be skipped here.
    continuation::Then(
        [ctx]() {
            continuation::While(
                [ctx]() { return ctx->next_block != nullptr; },
                [ctx]() { ctx->next_block->Run(ctx); });
        },
        [ctx, saved]() { ctx->next_block = saved; });
}

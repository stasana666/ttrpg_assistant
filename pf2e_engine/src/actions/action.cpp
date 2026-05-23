#include <action.h>

#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/common/continuation.h>
#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/player.h>

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

    // Pre-load declaratively-defined variables (e.g. a natural-attack weapon).
    for (const auto& variable : variables_) {
        ctx->game_object_registry->Add(variable.id, TGameObjectPtr{variable.weapon.get()});
    }

    Consume(player);

    ctx->io_system->GameLog() << player.GetName() << ": " << name_ << std::endl;

    // The pipeline is a linked list of blocks; each block sets ctx->next_block.
    // continuation::While drives it so that a block which suspends (throws a
    // savepoint) resumes with the remaining blocks intact.
    continuation::While(
        [ctx]() { return ctx->next_block != nullptr; },
        [ctx]() { ctx->next_block->Run(ctx); });
}

void TAction::Consume(TPlayer& player)
{
    for (auto& resource : consume_) {
        player.GetCreature()->Resources().Reduce(resource.resource_id, resource.count);
    }
}

bool TAction::Check(const TPlayer& player)
{
    for (auto& resource : consume_) {
        if (!player.GetCreature()->Resources().HasResource(resource.resource_id, resource.count)) {
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
    continuation::While(
        [ctx]() { return ctx->next_block != nullptr; },
        [ctx]() { ctx->next_block->Run(ctx); });
    ctx->next_block = saved;
}

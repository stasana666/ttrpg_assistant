#include <action.h>

#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/interaction_system.h>
#include <pf2e_engine/player.h>

const TGameObjectId kSelf = TGameObjectIdManager::Instance().Register("self");

TAction::TAction(TPipeline&& pipeline, TResources&& consume, std::string&& name)
    : pipeline_(std::move(pipeline))
    , consume_(std::move(consume))
    , name_(std::move(name))
{
}

void TAction::Apply(TActionContext ctx, TPlayer& player)
{
    TGameObjectRegistry game_object_registry;
    ctx.next_block = pipeline_.begin()->get();
    ctx.game_object_registry = &game_object_registry;

    ctx.game_object_registry->Add(kSelf, &player);

    Consume(player);

    ctx.io_system->GameLog() << player.name << ": " << name_ << std::endl;

    while (ctx.next_block != nullptr) {
        ctx.next_block->Apply(ctx);
    }
}

void TAction::Consume(TPlayer& player)
{
    for (auto& resource : consume_) {
        player.creature->Resources().Reduce(resource.resource_id, resource.count);
    }
}

bool TAction::Check(const TPlayer& player)
{
    for (auto& resource : consume_) {
        if (!player.creature->Resources().HasResource(resource.resource_id, resource.count)) {
            return false;
        }
    }
    return true;
}

std::string_view TAction::Name() const
{
    return name_;
}

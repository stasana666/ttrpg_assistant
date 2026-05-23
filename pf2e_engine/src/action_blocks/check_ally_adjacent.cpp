#include <check_ally_adjacent.h>

#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/success_level.h>

#include <variant>

static const TGameObjectId kSelfId = TGameObjectIdManager::Instance().Register("self");
static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("target");

void FCheckAllyAdjacent::operator()(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer* self = std::get<TPlayer*>(input_.Get(kSelfId, ctx));
    TPlayer* target = std::get<TPlayer*>(input_.Get(kTargetId, ctx));

    auto battle_map = ctx->battle->BattleMap();
    TPosition target_pos = target->GetPosition();

    TPlayerList allies = ctx->battle->GetIfPlayers([&](const TPlayer* player) {
        return player != self
            && player->GetTeam() == self->GetTeam()
            && player->GetCreature()->IsAlive()
            && battle_map->InRadius(player->GetPosition(), 1, target_pos);
    });

    ESuccessLevel result = allies.empty() ? ESuccessLevel::Failure
                                          : ESuccessLevel::Success;
    ctx->game_object_registry->Add(output_, result);
}

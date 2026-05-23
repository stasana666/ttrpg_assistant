#include <check_ally_adjacent.h>

#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/success_level.h>

#include <variant>

static const TGameObjectId kSelfId = TGameObjectIdManager::Instance().Register("self");
static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("target");
static const TGameObjectId kCountId = TGameObjectIdManager::Instance().Register("count");

void FCheckAllyAdjacent::operator()(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer* self = std::get<TPlayer*>(input_.Get(kSelfId, ctx));
    TPlayer* target = std::get<TPlayer*>(input_.Get(kTargetId, ctx));
    int required = input_.GetNumber(kCountId);

    auto battle_map = ctx->battle->BattleMap();
    TPosition target_pos = target->GetPosition();

    TPlayerList allies = ctx->battle->GetIfPlayers([&](const TPlayer* player) {
        if (player == self
            || player->GetTeam() != self->GetTeam()
            || !player->GetCreature()->IsAlive())
        {
            return false;
        }
        int reach = player->GetCreature()->MaxWeaponReach();
        return battle_map->InRadius(player->GetPosition(), reach, target_pos);
    });

    ESuccessLevel result = static_cast<int>(allies.size()) >= required
        ? ESuccessLevel::Success
        : ESuccessLevel::Failure;
    ctx->game_object_registry->Add(output_, result);
}

#include <pf2e_engine/action_blocks/remove_condition.h>

#include <pf2e_engine/condition.h>
#include <pf2e_engine/effect_manager.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/player.h>

static const TGameObjectId kConditionId = TGameObjectIdManager::Instance().Register("condition");
static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("target");

void FRemoveCondition::operator ()(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer& target = *std::get<TPlayer*>(input_.Get(kTargetId, ctx));
    ECondition condition = ConditionFromString(input_.GetString(kConditionId));
    // Use ClearCondition so effect-manager-tracked conditions (Frightened, MAP)
    // also cancel any scheduled per-turn tasks; otherwise the task would
    // resurrect a decremented value on the next OnTurnStart.
    ctx->effect_manager->ClearCondition(&target, condition, *ctx->transformator);
}

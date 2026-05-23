#include <pf2e_engine/action_blocks/remove_condition.h>

#include <pf2e_engine/condition.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

static const TGameObjectId kConditionId = TGameObjectIdManager::Instance().Register("condition");
static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("target");

void FRemoveCondition::operator ()(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer& target = *std::get<TPlayer*>(input_.Get(kTargetId, ctx));
    ECondition condition = ConditionFromString(input_.GetString(kConditionId));
    ctx->transformator->ChangeCondition(target.GetCreature(), condition, 0);
}

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
    EConditionKind condition = EConditionKindFromString(input_.GetString(kConditionId));
    ctx->effect_manager->ClearCondition(&target, condition, *ctx->transformator);
}

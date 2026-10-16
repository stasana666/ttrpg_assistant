#include <pf2e_engine/actions/generated_initiative.h>

#include <pf2e_engine/battle.h>

void MoveActionEntityBeforeCurrentTurn(const std::shared_ptr<TActionContext>& context,
                                       const TGeneratedEntityView& entity)
{
    ValidateActionActor(context);
    ValidateAbilitySource(context);
    ValidateActionEntity(context, entity);
    auto& player = entity.MutablePlayer();
    context->battle->MoveBeforeCurrentTurn(player, *context->transformator);
}

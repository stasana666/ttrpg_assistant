#include <pf2e_engine/actions/generated_movement.h>

#include <pf2e_engine/actions/generated_world.h>

#include <algorithm>

void MoveActionEntity(const std::shared_ptr<TActionContext>& context,
                      const TGeneratedEntityView& entity, TPosition expected_from,
                      TPosition destination)
{
    ValidateActionActor(context);
    ValidateAbilitySource(context);
    ValidateActionEntity(context, entity);
    auto& player = entity.MutablePlayer();
    const auto world = GeneratedActionWorld(context);
    if (world.Position(entity) != expected_from) {
        throw TAbilityUnavailable();
    }
    const auto adjacent = world.AdjacentPositions(expected_from);
    if (std::ranges::find(adjacent, destination) == adjacent.end()
        || !world.CanOccupy(entity, destination)) {
        throw TAbilityUnavailable();
    }
    context->transformator->MovePlayer(&player, destination);
}

#include <move.h>

#include <pf2e_engine/player.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("creature");
static const TGameObjectId kMovementId = TGameObjectIdManager::Instance().Register("movement");

void FMove::operator() (TActionContext& ctx) const
{
    TPlayer& target = *std::get<TPlayer*>(input_.Get(kTargetId, ctx));
    int movement = std::get<int>(input_.Get(kMovementId, ctx));

    while (movement > 0) {
        ++target.position.x;
        --movement;
    }
}

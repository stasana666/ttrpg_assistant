#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/creature.h>

void TGameObjectRegistry::Add(TGameObjectId id, TGameObjectPtr object)
{
    objects_.insert({id, object});
}

TGameObjectPtr TGameObjectRegistry::GetGameObjectPtr(TGameObjectId id)
{
    return objects_.at(id);
}

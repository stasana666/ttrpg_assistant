#include <pf2e_engine/game_object_logic/game_object_storage.h>

void TGameObjectStorage::Add(TGameObjectId id, TCreature* creature)
{
    creatures_.insert({id, creature});
}

TCreature* TGameObjectStorage::Get(TGameObjectId id)
{
    return creatures_.at(id);
}

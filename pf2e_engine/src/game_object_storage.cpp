#include "game_object_storage.h"

void TGameObjectStorage::Add(TGameObjectId id, TCreature* creature)
{
    creatures.insert({id, creature});
}

TCreature* TGameObjectStorage::Get(TGameObjectId id)
{
    return creatures.at(id);
}

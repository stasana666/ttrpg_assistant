#include <pf2e_engine/game_object_logic/game_object_storage.h>

void TGameObjectStorage::Add(TGameObjectId id, TGameObject object)
{
    objects_.insert({id, object});
}

TGameObject& TGameObjectStorage::GetRef(TGameObjectId id)
{
    return objects_.at(id);
}

TGameObject* TGameObjectStorage::GetPointer(TGameObjectId id)
{
    return &objects_.at(id);
}

TGameObject TGameObjectStorage::GetCopy(TGameObjectId id)
{
    return objects_.at(id);
}

#pragma once

#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object.h>

class TGameObjectStorage {
public:
    void Add(TGameObjectId id, TGameObject object);
    TGameObject& GetRef(TGameObjectId id);
    TGameObject* GetPointer(TGameObjectId id);
    TGameObject GetCopy(TGameObjectId id);

private:
    std::unordered_map<TGameObjectId, TGameObject, TGameObjectIdHash> objects_;
};

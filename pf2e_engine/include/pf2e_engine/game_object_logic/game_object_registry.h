#pragma once

#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object.h>

class TGameObjectRegistry {
public:
    void Add(TGameObjectId id, TGameObjectPtr object);

    template <class T>
    T& Get(TGameObjectId id);
    TGameObjectPtr GetGameObjectPtr(TGameObjectId id);

private:
    std::unordered_map<TGameObjectId, TGameObjectPtr, TGameObjectIdHash> objects_;
};

template <class T>
T& TGameObjectRegistry::Get(TGameObjectId id)
{
    auto game_object_ptr = objects_.at(id);
    return *std::get<T*>(game_object_ptr);
}

#pragma once

#include "game_object_id.h"

#include <pf2e_engine/creature.h>

class TGameObjectStorage {
public:
    void Add(TGameObjectId id, TCreature* creature);
    TCreature* Get(TGameObjectId id);

private:
    std::unordered_map<TGameObjectId, TCreature*, TGameObjectIdHash> creatures_;
};

#pragma once

#include <pf2e_engine/common/value_id.h>

#include "creature.h"

struct TGameObjectTag;

using TGameObjectId = TValueId<TGameObjectTag>;
using TGameObjectManager = TValueManager<TGameObjectTag>;

class TGameObjectStorage {
public:
    void Add(TGameObjectId id, TCreature* creature);
    TCreature* Get(TGameObjectId id);

private:
    std::unordered_map<TGameObjectId, TCreature*, TValueHash<TGameObjectTag>> creatures;
};

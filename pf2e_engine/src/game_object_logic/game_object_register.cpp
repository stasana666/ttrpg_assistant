#include <pf2e_engine/game_object_logic/game_object_register.h>
#include "creature.h"

void TGameObjectRegister::Add(TGameObjectId id, TGameObjectPtr object)
{
    objects_.insert({id, object});
}

TGameObjectPtr TGameObjectRegister::GetGameObjectPtr(TGameObjectId id)
{
    return objects_.at(id);
}

#include "hitpoints.h"
#include <creature.h>

TCreature::TCreature(TCharacteristicSet stats, TArmour armour, THitPoints hitpoints)
    : stats(stats)
    , armour(armour)
    , hitpoints(hitpoints)
{
    armourClass.Bind(this);
}

const TArmour& TCreature::GetArmour() const
{
    return armour;
}

const TCharacteristic& TCreature::GetCharacteristic(ECharacteristic name) const
{
    return stats[name];
}

int TCreature::GetAc() const
{
    return armourClass.GetAc();
}

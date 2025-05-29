#include <creature.h>

TCreature::TCreature(TCharacteristicSet stats, TArmor armor, THitPoints hitpoints)
    : stats(stats)
    , armor(armor)
    , armorSlot(&armor)
    , hitpoints(hitpoints)
{
}

const TArmor& TCreature::GetArmor() const
{
    return armor;
}

const TCharacteristic& TCreature::GetCharacteristic(ECharacteristic name) const
{
    return stats[name];
}

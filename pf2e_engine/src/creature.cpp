#include <creature.h>

TCreature::TCreature(TCharacteristicSet stats, TArmor armor, THitPoints hitpoints)
    : stats_(stats)
    , hitpoints_(hitpoints)
{
}

const TCharacteristic& TCreature::GetCharacteristic(ECharacteristic name) const
{
    return stats_[name];
}

TCharacteristicSet& TCreature::Characteristics()
{
    return stats_;
}

THitPoints& TCreature::Hitpoints()
{
    return hitpoints_;
}

TResourcePool& TCreature::Resources()
{
    return resources_;
}

TArmor& TCreature::Armor()
{
    return armor_;
}

TWeaponSlots& TCreature::Weapons()
{
    return weapons_;
}

#pragma once

#include <pf2e_engine/weapon_slot.h>

#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/mechanics/hitpoints.h>

class TCreature {
public:
    TCreature(TCharacteristicSet stats, TArmor armor, THitPoints hitpoint);

    const TCharacteristic& GetCharacteristic(ECharacteristic name) const;

    TCharacteristicSet& Characteristics();
    THitPoints& Hitpoints();
    TResourcePool& Resources();

    const TArmor& Armor() const;
    TWeaponSlots& Weapons();

private:
    TCharacteristicSet stats_;
    THitPoints hitpoints_;

    TResourcePool resources_;

    TArmor armor_;
    TWeaponSlots weapons_;
};

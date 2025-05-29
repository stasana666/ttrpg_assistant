#pragma once

#include <pf2e_engine/armor_slot.h>
#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/mechanics/hitpoints.h>

class TCreature {
public:
    TCreature(TCharacteristicSet stats, TArmor armor, THitPoints hitpoint);

    const TArmor& GetArmor() const;
    const TCharacteristic& GetCharacteristic(ECharacteristic name) const;

private:
    TCharacteristicSet stats;
    THitPoints hitpoints;

    TArmor armor;
    TArmorSlot armorSlot;
};

#pragma once

#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/mechanics/hitpoints.h>
#include <pf2e_engine/mechanics/characteristics.h>

#include "armor_class.h"

class TCreature {
public:
    TCreature(TCharacteristicSet stats, TArmor armor, THitPoints hitpoint);

    const TArmor& GetArmor() const;
    const TCharacteristic& GetCharacteristic(ECharacteristic name) const;

    int GetAc() const;

private:
    TCharacteristicSet stats;
    THitPoints hitpoints;

    TArmor armor;
    TArmorSlot armorSlot;
    TArmorClass armorClass;
};

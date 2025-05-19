#pragma once

#include <pf2e_engine/inventory/armour.h>
#include <pf2e_engine/mechanics/hitpoints.h>
#include <pf2e_engine/mechanics/characteristics.h>

#include "armour_class.h"

class TCreature {
public:
    TCreature(TCharacteristicSet stats, TArmour armour, THitPoints hitpoint);

    const TArmour& GetArmour() const;
    const TCharacteristic& GetCharacteristic(ECharacteristic name) const;

    int GetAc() const;

private:
    TCharacteristicSet stats;
    THitPoints hitpoints;
    TArmour armour;
    TArmourClass armourClass;
};

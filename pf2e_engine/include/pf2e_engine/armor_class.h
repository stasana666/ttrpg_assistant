#pragma once

#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/armor_slot.h>

class TArmorClass {
public:
    TArmorClass(TArmorSlot& armor, TCharacteristic& dexterity);

    int GetAc() const;

private:
    int ac_bonus;
    int dex_cap;
    int dex;
};

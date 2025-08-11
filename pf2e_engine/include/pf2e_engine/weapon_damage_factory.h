#pragma once

#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/weapon_slot.h>

class TWeaponDamageFactory {
public:
    TWeaponDamageFactory(TWeaponSlot& weapon, TCharacteristic& strength);

    TDamage HitDamage() const;
    TDamage CritDamage() const;

private:
    int str_;
    const TWeapon* weapon_;
};

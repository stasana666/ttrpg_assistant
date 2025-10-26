#pragma once

#include <pf2e_engine/weapon_slot.h>

#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/mechanics/hitpoints.h>
#include <pf2e_engine/mechanics/damage_resolver.h>
#include <pf2e_engine/actions/action.h>

class TCreature {
public:
    TCreature(TCharacteristicSet stats, TArmor armor, THitPoints hitpoint);

    const TCharacteristic& GetCharacteristic(ECharacteristic name) const;

    TCharacteristicSet& Characteristics();
    THitPoints& Hitpoints();
    TResourcePool& Resources();

    const TDamageResolver& DamageResolver() const;

    const TArmor& Armor() const;
    TWeaponSlots& Weapons();

    bool IsAlive() const;
    void AddAction(std::shared_ptr<TAction> action);
    std::vector<std::shared_ptr<TAction>>& Actions();

private:
    TCharacteristicSet stats_;
    THitPoints hitpoints_;
    TDamageResolver resolver_;

    TResourcePool resources_;

    TArmor armor_;
    TWeaponSlots weapons_;
    std::vector<std::shared_ptr<TAction>> actions_;
};

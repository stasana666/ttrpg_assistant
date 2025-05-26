#pragma once

#include <pf2e_engine/common/observable.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/resources.h>

class TWeaponSlot final : public TObservable<const TWeapon*> {
public:
    explicit TWeaponSlot(TResourcePool& pool);

    void Equip(const TWeapon*);
    bool Has() const;
    const TWeapon* Get() const;
    void Release();

private:
    const TWeapon* weapon;
    TResourcePool& pool;
};

#include <weapon_slot.h>

TWeaponSlot::TWeaponSlot(TResourcePool& pool)
    : weapon(nullptr)
    , pool(pool)
{
}

void TWeaponSlot::Equip(const TWeapon* w)
{
    weapon = w;
    NotifyAll(weapon);
}

bool TWeaponSlot::Has() const
{
    return weapon != nullptr;
}

const TWeapon* TWeaponSlot::Get() const
{
    return weapon;
}

void TWeaponSlot::Release()
{
    weapon = nullptr;
    NotifyAll(nullptr);
}

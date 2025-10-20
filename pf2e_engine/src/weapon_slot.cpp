#include <weapon_slot.h>

auto TWeaponSlots::Equip(THoldedWeapon weapon) -> TWeaponDescriptor
{
    weapons_.emplace_back(weapon);
    return TWeaponDescriptor(this, weapons_.size() - 1);
}

auto TWeaponSlots::operator [](size_t idx) -> TWeaponDescriptor
{
    return TWeaponDescriptor(this, idx);
}

size_t TWeaponSlots::Size() const
{
    return weapons_.size();
}

bool TWeaponSlots::Empty() const
{
    return weapons_.empty();
}

TWeaponSlots::TWeaponDescriptor::TWeaponDescriptor(TWeaponSlots* parent, size_t index)
    : parent_(parent)
    , index_(index)
{
}

void TWeaponSlots::TWeaponDescriptor::SetGrip(size_t hand_count)
{
    parent_->weapons_[index_].hand_count = hand_count;
}

int TWeaponSlots::TWeaponDescriptor::Grip() const
{
    return parent_->weapons_[index_].hand_count;
}

TWeapon& TWeaponSlots::TWeaponDescriptor::Weapon()
{
    return parent_->weapons_[index_].weapon;
}

const TWeapon& TWeaponSlots::TWeaponDescriptor::Weapon() const
{
    return parent_->weapons_[index_].weapon;
}

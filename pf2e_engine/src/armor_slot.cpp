#include <armor_slot.h>

TArmorSlot::TArmorSlot()
    : armor(&TArmor::GetDefault())
{
}

TArmorSlot::TArmorSlot(const TArmor* armor)
    : armor(armor)
{
}

const TArmor& TArmorSlot::Get() const
{
    return *armor;
}

void TArmorSlot::Set(const TArmor* newArmor)
{
    armor = newArmor;
    NotifyAll(*armor);
}

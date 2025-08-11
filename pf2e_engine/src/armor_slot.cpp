#include <armor_slot.h>

TArmorSlot::TArmorSlot()
{
}

TArmorSlot::TArmorSlot(const TArmor* armor)
    : armor_(armor_)
{
}

const TArmor& TArmorSlot::Get() const
{
    return *armor_;
}

void TArmorSlot::Set(const TArmor* new_armor)
{
    armor_ = new_armor;
    NotifyAll(*armor_);
}

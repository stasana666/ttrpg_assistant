#include <weapon.h>

TWeapon::TWeapon(int base_die_size, TDamage::Type type)
    : base_die_size(base_die_size)
    , type(type)
{
}

int TWeapon::GetBaseDieSize() const
{
    return base_die_size;
}

TDamage::Type TWeapon::GetDamageType() const
{
    return type;
}

#include <weapon.h>

TWeapon::TWeapon(int base_die_size, TDamage::Type type)
    : base_die_size_(base_die_size)
    , type_(type)
{
}

int TWeapon::GetBaseDieSize() const
{
    return base_die_size_;
}

TDamage::Type TWeapon::GetDamageType() const
{
    return type_;
}

std::vector<int> TWeapon::Grips() const
{
    return {1};
}

bool TWeapon::ValidGrip(int hand_count) const
{
    for (int i : Grips()) {
        if (i == hand_count) {
            return true;
        }
    }
    return false;
}

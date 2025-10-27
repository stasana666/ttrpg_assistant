#include <weapon.h>


std::string ToString(EWeaponCategory proficiency)
{
    switch (proficiency) {
        case EWeaponCategory::Unarmed:
            return "Unarmed";
        case EWeaponCategory::Simple:
            return "Simple";
        case EWeaponCategory::Martial:
            return "Martial";
    }
    throw std::runtime_error("incorrect value of EWeaponCategory");
}

EWeaponCategory WeaponCategoryFromString(std::string str_category)
{
    for (auto category : 
        { EWeaponCategory::Unarmed
        , EWeaponCategory::Simple
        , EWeaponCategory::Martial})
    {
        if (ToString(category) == str_category) {
            return category;
        }
    }
    throw std::runtime_error("unknown EWeaponCategory: \"" + str_category + "\"");
}

TWeapon::TWeapon(int base_die_size, TDamage::Type type, EWeaponCategory category)
    : base_dice_size_(base_die_size)
    , type_(type)
    , category_(category)
{
}

EWeaponCategory TWeapon::WeaponCategory() const
{
    return category_;
}

int TWeapon::GetBaseDiceSize() const
{
    return base_dice_size_;
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

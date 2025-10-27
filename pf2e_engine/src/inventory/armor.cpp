#include "armor.h"
#include <stdexcept>

std::string ToString(EArmorCategory armor_category)
{
    switch (armor_category) {
        case EArmorCategory::Unarmored:
            return "Unarmored";
        case EArmorCategory::Light:
            return "Light";
        case EArmorCategory::Medium:
            return "Medium";
        case EArmorCategory::Heavy:
            return "Heavy";
    }
    throw std::runtime_error("incorrect value of EArmorCategory");
}

EArmorCategory ArmorCategoryFromString(std::string str_category)
{
    for (auto category : 
        { EArmorCategory::Unarmored
        , EArmorCategory::Light
        , EArmorCategory::Medium
        , EArmorCategory::Heavy})
    {
        if (ToString(category) == str_category) {
            return category;
        }
    }
    throw std::runtime_error("unknown EArmorCategory: \"" + str_category + "\"");
}

int TArmor::AcBonus() const
{
    return ac_bonus_;
}

int TArmor::DexCap() const
{
    return dex_cap_;
}

EArmorCategory TArmor::ArmorCategory() const
{
    return category_;
}

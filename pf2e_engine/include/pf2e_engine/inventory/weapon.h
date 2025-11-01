#pragma once

#include <pf2e_engine/mechanics/damage.h>
#include <vector>
#include "item.h"

enum class EWeaponCategory {
    Unarmed,
    Simple,
    Martial
};

std::string ToString(EWeaponCategory weapon_category);
EWeaponCategory WeaponCategoryFromString(std::string weapon_category);

class TWeapon : public TItem {
public:
    TWeapon(std::string_view name, int base_dice_size, TDamage::Type type, EWeaponCategory category);

    EWeaponCategory WeaponCategory() const;
    int GetBaseDiceSize() const;
    TDamage::Type GetDamageType() const;
    std::vector<int> Grips() const;
    bool ValidGrip(int hand_count) const;
    std::string_view Name() const;

private:
    int base_dice_size_;
    TDamage::Type type_;
    EWeaponCategory category_;
    std::string_view name_;
};

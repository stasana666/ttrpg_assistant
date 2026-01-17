#pragma once

#include <pf2e_engine/mechanics/damage.h>
#include <pf2e_engine/inventory/item.h>
#include <pf2e_engine/traits_haver.h>

#include <vector>

enum class EWeaponCategory {
    Unarmed,
    Simple,
    Martial
};

std::string ToString(EWeaponCategory weapon_category);
EWeaponCategory WeaponCategoryFromString(std::string weapon_category);

enum class EWeaponTrait {
    Agile,
    Finesse,
    COUNT
};

std::string ToString(EWeaponTrait weapon_trait);
EWeaponTrait WeaponTraitFromString(std::string weapon_trait);

// Backward compatibility: TWeaponTrait is now an alias for TTrait<EWeaponTrait>
using TWeaponTrait = TTrait<EWeaponTrait>;

class TWeapon : public TItem, public TTraitsHaver<EWeaponTrait> {
public:
    TWeapon(std::string_view name, int base_dice_size, TDamage::Type type, EWeaponCategory category);

    EWeaponCategory WeaponCategory() const;
    int GetBaseDiceSize() const;
    TDamage::Type GetDamageType() const;
    std::vector<int> Grips() const;
    bool ValidGrip(int hand_count) const;
    std::string_view Name() const;

private:
    friend class TGameObjectFactory;

    int base_dice_size_;
    TDamage::Type type_;
    EWeaponCategory category_;
    std::string name_;
};

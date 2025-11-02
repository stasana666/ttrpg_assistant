#pragma once

#include <pf2e_engine/mechanics/damage.h>
#include <pf2e_engine/inventory/item.h>

#include <vector>
#include <variant>
#include <set>

enum class EWeaponCategory {
    Unarmed,
    Simple,
    Martial
};

std::string ToString(EWeaponCategory weapon_category);
EWeaponCategory WeaponCategoryFromString(std::string weapon_category);

enum class EWeaponTrait {
    Agile,
    COUNT
};

std::string ToString(EWeaponTrait weapon_trait);
EWeaponTrait WeaponTraitFromString(std::string weapon_trait);

using TTraitValue = std::variant<std::monostate, int>;

struct TWeaponTrait {
    EWeaponTrait type;
    TTraitValue value;
};

class TWeapon : public TItem {
public:
    TWeapon(std::string_view name, int base_dice_size, TDamage::Type type, EWeaponCategory category);

    EWeaponCategory WeaponCategory() const;
    int GetBaseDiceSize() const;
    TDamage::Type GetDamageType() const;
    std::vector<int> Grips() const;
    bool ValidGrip(int hand_count) const;
    std::string_view Name() const;

    const std::vector<TWeaponTrait>& Traits() const;
    bool HasTrait(EWeaponTrait trait) const;
    void AddTrait(TWeaponTrait trait);

private:
    friend class TGameObjectFactory;

    int base_dice_size_;
    TDamage::Type type_;
    EWeaponCategory category_;
    std::string_view name_;
    std::vector<TWeaponTrait> traits_;
};

#include <weapon.h>

#include <nlohmann/json.hpp>

#include <stdexcept>


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

std::string ToString(EWeaponTrait weapon_trait)
{
    switch (weapon_trait) {
        case EWeaponTrait::Agile:
            return "Agile";
        case EWeaponTrait::Finesse:
            return "Finesse";
        case EWeaponTrait::COUNT:
            throw std::runtime_error("COUNT is not valid EWeaponTrait");
    }
    throw std::runtime_error("incorrect value of EWeaponTrait");
}

EWeaponTrait WeaponTraitFromString(std::string weapon_trait)
{
    for (size_t i = 0; i < static_cast<size_t>(EWeaponTrait::COUNT); ++i) {
        if (ToString(static_cast<EWeaponTrait>(i)) == weapon_trait) {
            return static_cast<EWeaponTrait>(i);
        }
    }
    throw std::runtime_error("unknown EWeaponTrait: \"" + weapon_trait + "\"");
}

TWeapon::TWeapon(std::string_view name, int base_die_size, TDamage::Type type, EWeaponCategory category)
    : base_dice_size_(base_die_size)
    , type_(type)
    , category_(category)
    , name_(name)
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

std::string_view TWeapon::Name() const
{
    return name_;
}

// Trait methods (Traits, HasTrait, AddTrait) are now inherited from TTraitsHaver<EWeaponTrait>

TWeapon WeaponFromJson(std::string_view name, const nlohmann::json& weapon_core)
{
    int base_die_size = weapon_core["base_die_size"];
    TDamage::Type damage_type = DamageTypeFromString(std::string{weapon_core["damage_type"]});
    EWeaponCategory category = WeaponCategoryFromString(weapon_core["category"]);
    TWeapon result(name, base_die_size, damage_type, category);

    if (weapon_core.contains("traits")) {
        for (const auto& trait_json : weapon_core["traits"]) {
            std::string trait_name = trait_json["name"];
            EWeaponTrait trait_type = WeaponTraitFromString(trait_name);

            TWeaponTrait trait;
            trait.type = trait_type;
            if (trait_json.contains("value")) {
                trait.value = trait_json["value"].get<int>();
            } else {
                trait.value = std::monostate{};
            }
            result.AddTrait(trait);
        }
    }

    return result;
}

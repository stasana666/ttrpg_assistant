#pragma once

#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/weapon.h>

#include <variant>
#include <map>

enum class EProficiencyLevel {
    Untrained,
    Trained,
    Expert,
    Master,
    Legendary,
};

std::string ToString(EProficiencyLevel proficiency);
EProficiencyLevel ProficiencyLevelFromString(std::string proficiency);

class TProficiency {
public:
    using Value = std::variant<EProficiencyLevel, int>;

    explicit TProficiency(int level);

    void SetProficiency(EArmorCategory, Value);
    void SetProficiency(EWeaponCategory, Value);

    int GetProficiency(const TWeapon&) const;
    int GetProficiency(const TArmor&) const;
    int GetLevel() const;

private:
    int GetProficiency(EProficiencyLevel proficiency) const;

    int level_;
    std::map<EArmorCategory, Value> armor_category_;
    std::map<EWeaponCategory, Value> weapon_category_;
};

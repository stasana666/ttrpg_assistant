#pragma once

#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/weapon.h>

#include <pf2e_engine/mechanics/savethrows.h>
#include <pf2e_engine/mechanics/skills.h>

#include <variant>
#include <map>
#include "pf2e_engine/mechanics/characteristics.h"

enum class EProficiencyLevel {
    Untrained,
    Trained,
    Expert,
    Master,
    Legendary,
};

std::string ToString(EProficiencyLevel proficiency);
EProficiencyLevel ProficiencyLevelFromString(std::string proficiency);

struct TPerceptionTag {};

ECharacteristic BindedCharacteristic(TPerceptionTag);

class TProficiency {
public:
    using Value = std::variant<EProficiencyLevel, int>;

    explicit TProficiency(int level);

    void SetProficiency(EArmorCategory, Value);
    void SetProficiency(EWeaponCategory, Value);
    void SetProficiency(ESavethrow, Value);
    void SetProficiency(ESkill, Value);
    void SetProficiency(TPerceptionTag, Value);

    int GetProficiency(const TWeapon&) const;
    int GetProficiency(const TArmor&) const;
    int GetProficiency(ESavethrow) const;
    int GetProficiency(ESkill) const;
    int GetProficiency(TPerceptionTag) const;
    int GetLevel() const;

private:
    int GetProficiency(Value proficiency) const;
    int GetProficiency(EProficiencyLevel proficiency) const;

    int level_;
    std::map<EArmorCategory, Value> armor_category_;
    std::map<EWeaponCategory, Value> weapon_category_;
    std::map<ESavethrow, Value> savethrow_;
    std::map<ESkill, Value> skill_;
    Value perception_{0};
};

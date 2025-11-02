#include <pf2e_engine/proficiency.h>

#include <pf2e_engine/common/visit.h>
#include <stdexcept>
#include "savethrows.h"
#include "skills.h"

std::string ToString(EProficiencyLevel proficiency)
{
    switch (proficiency) {
        case EProficiencyLevel::Untrained:
            return "untrained";
        case EProficiencyLevel::Trained:
            return "trained";
        case EProficiencyLevel::Expert:
            return "expert";
        case EProficiencyLevel::Master:
            return "master";
        case EProficiencyLevel::Legendary:
            return "legendary";
    }
    throw std::invalid_argument("incorrect value of EProficiencyLevel");
}

EProficiencyLevel ProficiencyLevelFromString(std::string proficiency_str)
{
    for (auto proficiency :
        { EProficiencyLevel::Untrained
        , EProficiencyLevel::Trained
        , EProficiencyLevel::Expert
        , EProficiencyLevel::Master
        , EProficiencyLevel::Legendary})
    {
        if (ToString(proficiency) == proficiency_str) {
            return proficiency;
        }
    }
    throw std::invalid_argument("unknown EProficiencyLevel: \"" + proficiency_str + "\"");
}

TProficiency::TProficiency(int level)
    : level_(level)
{
}

void TProficiency::SetProficiency(EArmorCategory armor_category, Value value)
{
    armor_category_[armor_category] = value;
}

void TProficiency::SetProficiency(EWeaponCategory weapon_category, Value value)
{
    weapon_category_[weapon_category] = value;
}

void TProficiency::SetProficiency(ESavethrow savethrows, Value value)
{
    savethrow_[savethrows] = value;
}

void TProficiency::SetProficiency(ESkill skill, Value value)
{
    skill_[skill] = value;
}

void TProficiency::SetProficiency(TPerceptionTag, Value value)
{
    perception_ = value;
}

int TProficiency::GetProficiency(const TWeapon& weapon) const
{
    if (!weapon_category_.contains(weapon.WeaponCategory())) {
        return 0;
    }
    return GetProficiency(weapon_category_.at(weapon.WeaponCategory()));
}

int TProficiency::GetProficiency(const TArmor& armor) const
{
    if (!armor_category_.contains(armor.ArmorCategory())) {
        return 0;
    }
    return GetProficiency(armor_category_.at(armor.ArmorCategory()));
}

int TProficiency::GetProficiency(ESavethrow savethrow) const
{
    if (!savethrow_.contains(savethrow)) {
        return 0;
    }
    return GetProficiency(savethrow_.at(savethrow));
}

int TProficiency::GetProficiency(ESkill skill) const
{
    if (!skill_.contains(skill)) {
        return 0;
    }
    return GetProficiency(skill_.at(skill));
}

int TProficiency::GetProficiency(TPerceptionTag) const
{
    return GetProficiency(perception_);
}

int TProficiency::GetProficiency(Value proficiency) const
{
    int result{};
    std::visit(VisitorHelper{
        [&result, this](EProficiencyLevel proficiency) {
            result = GetProficiency(proficiency);
        },
        [&result](int value) {
            result = value;
        }
    }, proficiency);
    return result;
}

int TProficiency::GetProficiency(EProficiencyLevel proficiency) const
{
    switch (proficiency) {
        case EProficiencyLevel::Untrained:
            return 0;
        case EProficiencyLevel::Trained:
            return 2 + level_;
        case EProficiencyLevel::Expert:
            return 4 + level_;
        case EProficiencyLevel::Master:
            return 6 + level_;
        case EProficiencyLevel::Legendary:
            return 8 + level_;
    }
    throw std::invalid_argument("wrong EProficiencyLevel value in TProficiency::GetProficiency");
}

int TProficiency::GetLevel() const
{
    return level_;
}

#include <pf2e_engine/proficiency.h>

#include <pf2e_engine/common/visit.h>
#include <stdexcept>

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
    throw std::runtime_error("incorrect value of EProficiencyLevel");
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
    throw std::runtime_error("unknown EProficiencyLevel: \"" + proficiency_str + "\"");
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

int TProficiency::GetProficiency(const TWeapon& weapon) const
{
    if (!weapon_category_.contains(weapon.WeaponCategory())) {
        return 0;
    }
    int result{};
    std::visit(VisitorHelper{
        [&result, this](EProficiencyLevel proficiency) {
            result = GetProficiency(proficiency);
        },
        [&result](int value) {
            result = value;
        }
    }, weapon_category_.at(weapon.WeaponCategory()));
    return result;
}

int TProficiency::GetProficiency(const TArmor& armor) const
{
    if (!armor_category_.contains(armor.ArmorCategory())) {
        return 0;
    }
    int result{};
    std::visit(VisitorHelper{
        [&result, this](EProficiencyLevel proficiency) {
            result = GetProficiency(proficiency);
        },
        [&result](int value) {
            result = value;
        }
    }, armor_category_.at(armor.ArmorCategory()));
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
    throw std::logic_error("wrong EProficiencyLevel value in TProficiency::GetProficiency");
}

int TProficiency::GetLevel() const
{
    return level_;
}

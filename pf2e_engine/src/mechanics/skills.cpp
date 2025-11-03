#include <pf2e_engine/mechanics/skills.h>

#include <pf2e_engine/mechanics/characteristics.h>

#include <stdexcept>

std::string ToString(ESkill skill)
{
    switch (skill) {
        case ESkill::Acrobatics:
            return "Acrobatics";
        case ESkill::Arcana:
            return "Arcana";
        case ESkill::Athletics:
            return "Athletics";
        case ESkill::Crafting:
            return "Crafting";
        case ESkill::Deception:
            return "Deception";
        case ESkill::Diplomacy:
            return "Diplomacy";
        case ESkill::Intimidation:
            return "Intimidation";
        case ESkill::Medicine:
            return "Medicine";
        case ESkill::Nature:
            return "Nature";
        case ESkill::Occultism:
            return "Occultism";
        case ESkill::Performance:
            return "Performance";
        case ESkill::Religion:
            return "Religion";
        case ESkill::Society:
            return "Society";
        case ESkill::Stealth:
            return "Stealth";
        case ESkill::Survival:
            return "Survival";
        case ESkill::Thievery:
            return "Thievery";
        case ESkill::COUNT:
            throw std::invalid_argument("COUNT is not valid ESkill");
    }
    throw std::invalid_argument("incorrect value of ESkill");
}

ESkill SkillFromString(std::string skill)
{
    for (size_t i = 0; i < static_cast<size_t>(ESkill::COUNT); ++i) {
        if (ToString(static_cast<ESkill>(i)) == skill) {
            return static_cast<ESkill>(i);
        }
    }
    throw std::invalid_argument("unknown ESkill: \"" + skill + "\"");
}

ECharacteristic BindedCharacteristic(ESkill skill)
{
        switch (skill) {
        case ESkill::Acrobatics:
            return ECharacteristic::Dexterity;
        case ESkill::Arcana:
            return ECharacteristic::Intelligence;
        case ESkill::Athletics:
            return ECharacteristic::Strength;
        case ESkill::Crafting:
            return ECharacteristic::Intelligence;
        case ESkill::Deception:
            return ECharacteristic::Charisma;
        case ESkill::Diplomacy:
            return ECharacteristic::Charisma;
        case ESkill::Intimidation:
            return ECharacteristic::Charisma;
        case ESkill::Medicine:
            return ECharacteristic::Wisdom;
        case ESkill::Nature:
            return ECharacteristic::Wisdom;
        case ESkill::Occultism:
            return ECharacteristic::Intelligence;
        case ESkill::Performance:
            return ECharacteristic::Charisma;
        case ESkill::Religion:
            return ECharacteristic::Wisdom;
        case ESkill::Society:
            return ECharacteristic::Intelligence;
        case ESkill::Stealth:
            return ECharacteristic::Dexterity;
        case ESkill::Survival:
            return ECharacteristic::Wisdom;
        case ESkill::Thievery:
            return ECharacteristic::Dexterity;
        case ESkill::COUNT:
            throw std::invalid_argument("COUNT is not valid ESkill");
    }
    throw std::invalid_argument("incorrect value of ESkill");
}

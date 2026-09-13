#include <pf2e_engine/mechanics/skills.h>

#include <pf2e_engine/mechanics/characteristics.h>

#include <stdexcept>

ESkill SkillFromString(std::string skill)
{
    return ESkillFromString(skill);
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
    }
    throw std::invalid_argument("incorrect value of ESkill");
}

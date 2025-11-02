#pragma once

#include <pf2e_engine/mechanics/characteristics.h>

#include <string>

enum class ESkill {
    Acrobatics,
    Arcana,
    Athletics,
    Crafting,
    Deception,
    Diplomacy,
    Intimidation,
    Medicine,
    Nature,
    Occultism,
    Performance,
    Religion,
    Society,
    Stealth,
    Survival,
    Thievery,
    COUNT,
};

std::string ToString(ESkill skill);
ESkill SkillFromString(std::string str_savethrow);

constexpr ECharacteristic BindedCharacteristic(ESkill skill);

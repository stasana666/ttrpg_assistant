#pragma once

#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/inventory/skills.h>

#include <string>

ESkill SkillFromString(std::string skill);

ECharacteristic BindedCharacteristic(ESkill skill);

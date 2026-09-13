#pragma once

#include <pf2e_engine/inventory/proficiency.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/mechanics/skills.h>
#include <pf2e_engine/mechanics/savethrows.h>

class TCreature;

struct TPerceptionTag {};

ECharacteristic BindedCharacteristic(TPerceptionTag);

int ProficiencyBonus(const TCreature& creature, const TWeapon& weapon);
int ProficiencyBonus(const TCreature& creature, const TArmor& armor);
int ProficiencyBonus(const TCreature& creature, ESavethrow save);
int ProficiencyBonus(const TCreature& creature, ESkill skill);
int ProficiencyBonus(const TCreature& creature, TPerceptionTag);

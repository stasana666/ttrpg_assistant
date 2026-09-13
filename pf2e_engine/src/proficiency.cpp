#include <pf2e_engine/proficiency.h>

#include <pf2e_engine/creature.h>

ECharacteristic BindedCharacteristic(TPerceptionTag)
{
    return ECharacteristic::Wisdom;
}

int ProficiencyBonus(const TCreature& creature, const TWeapon& weapon)
{
    return creature.WeaponProficiency(weapon.Category());
}

int ProficiencyBonus(const TCreature& creature, const TArmor& armor)
{
    return creature.ArmorProficiency(armor.Category());
}

int ProficiencyBonus(const TCreature& creature, ESavethrow save)
{
    return creature.SaveProficiency(save);
}

int ProficiencyBonus(const TCreature& creature, ESkill skill)
{
    return creature.SkillProficiency(skill);
}

int ProficiencyBonus(const TCreature& creature, TPerceptionTag)
{
    return creature.PerceptionProficiency();
}

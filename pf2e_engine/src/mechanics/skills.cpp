#include <pf2e_engine/mechanics/skills.h>

#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/inventory/statistics.h>

ESkill SkillFromString(std::string skill)
{
    return ESkillFromString(skill);
}

ECharacteristic BindedCharacteristic(ESkill skill)
{
    return TStatisticRules::Create({}).SkillAttribute(skill);
}

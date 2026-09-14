#include <pf2e_engine/mechanics/creature.h>

#include <algorithm>
#include <stdexcept>

const TResource& ResourceFor(const TCreature& creature, EResourceKind kind)
{
    switch (kind) {
        case EResourceKind::Action: return creature.Actions();
        case EResourceKind::Reaction: return creature.Reactions();
    }
    throw std::logic_error("unreachable");
}

TGuarded<TResource> ResourceFor(TCreature& creature, EResourceKind kind)
{
    switch (kind) {
        case EResourceKind::Action: return creature.Actions();
        case EResourceKind::Reaction: return creature.Reactions();
    }
    throw std::logic_error("unreachable");
}

int ConditionValue(const TCreature& creature, EConditionKind condition)
{
    if (!creature.Conditions().Has(condition)) {
        return 0;
    }
    switch (condition) {
        case EConditionKind::Prone: return 1;
        case EConditionKind::Frightened:
            return creature.Conditions().Get<TConditionFrightened>()->Value;
        case EConditionKind::MultipleAttackPenalty:
            return creature.Conditions().Get<TConditionMultipleAttackPenalty>()->Value;
    }
    throw std::logic_error("unreachable");
}

int MaxWeaponReach(const TCreature& creature)
{
    int reach = 0;
    for (auto weapon : creature.Weapons()) {
        reach = std::max(reach, weapon->Reach());
    }
    for (auto weapon : creature.NaturalWeapons()) {
        reach = std::max(reach, weapon->Reach());
    }
    return reach;
}

#include <pf2e_engine/mechanics/creature.h>

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
        case EConditionKind::Unconscious: return 1;
        case EConditionKind::Frightened:
            return creature.Conditions().Get<TConditionFrightened>()->Value;
        case EConditionKind::Dying:
            return creature.Conditions().Get<TConditionDying>()->Value;
        case EConditionKind::Wounded:
            return creature.Conditions().Get<TConditionWounded>()->Value;
    }
    throw std::logic_error("unreachable");
}

int MaxWeaponReach(const TCreature& creature)
{
    return creature.MaxWeaponReach();
}

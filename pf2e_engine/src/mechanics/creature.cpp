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

int ConditionValue(const TVariantMap<EConditionKind, TCondition>& conditions, EConditionKind condition)
{
    if (!conditions.Has(condition)) {
        return 0;
    }
    switch (condition) {
        case EConditionKind::Prone:
        case EConditionKind::Unconscious:
            return 1;
        case EConditionKind::Frightened:
            return conditions.Get<TConditionFrightened>()->Value;
        case EConditionKind::Dying:
            return conditions.Get<TConditionDying>()->Value;
        case EConditionKind::Wounded:
            return conditions.Get<TConditionWounded>()->Value;
    }
    throw std::logic_error("unreachable");
}

int ConditionValue(const TCreature& creature, EConditionKind condition)
{
    return ConditionValue(creature.Conditions(), condition);
}

#pragma once

#include <pf2e_engine/creature.h>
#include <pf2e_engine/common/resource.h>
#include <pf2e_engine/actions/action.h>

const TResource& ResourceFor(const TCreature& creature, EResourceKind kind);
TGuarded<TResource> ResourceFor(TCreature& creature, EResourceKind kind);
int ConditionValue(const TCreature& creature, EConditionKind condition);
int MaxWeaponReach(const TCreature& creature);

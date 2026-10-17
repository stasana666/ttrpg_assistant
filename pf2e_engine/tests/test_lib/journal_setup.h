#pragma once

#include <pf2e_engine/creature.h>
#include <pf2e_engine/transformation/transformator.h>

#include <algorithm>
#include <utility>

inline void SetCondition(TTransformator& journal, TCreature& creature, TCondition condition)
{
    journal.ExecutionContext().SetVariant(
        creature.Conditions(), std::move(condition), TGuarded<TCreature>(creature), "conditions");
}

inline void AddResource(TTransformator& journal, TGuarded<TResource> resource, int count)
{
    journal.ExecutionContext().AddResource(resource, count, resource, "count");
}

inline void ReduceResource(TTransformator& journal, TGuarded<TResource> resource, int count)
{
    journal.ExecutionContext().SetResourceCount(
        resource, std::max(0, resource->Count() - count), resource, "count");
}

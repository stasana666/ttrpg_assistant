#pragma once

#include <pf2e_engine/alternatives.h>

#include <cstddef>

// Policy that picks a choice for an automated combatant. Implementations
// inspect the alternatives' kind ("next action", "target", ...) and pick an
// index without any human input.
class IDecisionStrategy {
public:
    virtual ~IDecisionStrategy() = default;

    virtual size_t Decide(int player_id, const TAlternatives& alternatives) const = 0;
};

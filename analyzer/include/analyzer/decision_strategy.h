#pragma once

#include <pf2e_engine/alternatives.h>

#include <cstddef>

class IDecisionStrategy {
public:
    virtual ~IDecisionStrategy() = default;

    virtual size_t Decide(int player_id, const TAlternatives& alternatives) const = 0;
};

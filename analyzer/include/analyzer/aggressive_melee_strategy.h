#pragma once

#include <analyzer/decision_strategy.h>

class TAggressiveMeleeStrategy : public IDecisionStrategy {
public:
    size_t Decide(int player_id, const TAlternatives& alternatives) const override;
};

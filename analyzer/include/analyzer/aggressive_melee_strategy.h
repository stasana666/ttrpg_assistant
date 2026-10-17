#pragma once

#include <analyzer/decision_strategy.h>

class TBattle;

class TAggressiveMeleeStrategy : public IDecisionStrategy {
public:
    void BindBattle(const TBattle& battle);
    size_t Decide(int player_id, const TAlternatives& alternatives) const override;

private:
    const TBattle* battle_ = nullptr;
};

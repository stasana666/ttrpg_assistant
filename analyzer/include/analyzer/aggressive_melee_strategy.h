#pragma once

#include <analyzer/decision_strategy.h>

// Always attacks: picks the weapon-attack action and an enemy target.
// Falls back to the first alternative ("End of turn" for the action choice)
// when no offensive option is available.
class TAggressiveMeleeStrategy : public IDecisionStrategy {
public:
    size_t Decide(int player_id, const TAlternatives& alternatives) const override;
};

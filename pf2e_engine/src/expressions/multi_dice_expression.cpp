#include "multi_dice_expression.h"

TMultiDiceExpression::TMultiDiceExpression(int count, int size)
    : count_(count)
    , size_(size)
{
}

int TMultiDiceExpression::Value(IRandomGenerator& rng) const
{
    int total = 0;
    for (int i = 0; i < count_; ++i) {
        total += rng.RollDice(size_);
    }
    return total;
}

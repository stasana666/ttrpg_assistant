#include "dice_expression.h"

#include <cassert>
#include "random.h"

TDiceExpression::TDiceExpression(int size)
    : size_(size)
{
}

int TDiceExpression::Value(IRandomGenerator& dice_roller) const
{
    return dice_roller.RollDice(size_);
}

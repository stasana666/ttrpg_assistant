#include "dice.h"

TSumExpression::TSumExpression(std::unique_ptr<IExpression>&& left, std::unique_ptr<IExpression>&& right)
    : leftOperand(std::move(left))
    , rightOperand(std::move(right))
{
}

int TSumExpression::Value(IRandomGenerator& rng) const
{
    return leftOperand->Value(rng) + rightOperand->Value(rng);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TDice::TDice(int size)
    : size(size)
{
}

int TDice::Value(IRandomGenerator& rng) const
{
    return rng.RollDice(size);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TNumber::TNumber(int value)
    : value(value)
{
}

int TNumber::Value(IRandomGenerator&) const
{
    return value;
}

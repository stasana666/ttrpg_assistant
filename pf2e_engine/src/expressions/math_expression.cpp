#include "math_expression.h"

TSumExpression::TSumExpression(std::unique_ptr<IExpression>&& left, std::unique_ptr<IExpression>&& right)
    : leftOperand(std::move(left))
    , rightOperand(std::move(right))
{
}

int TSumExpression::Value(IRandomGenerator& rng) const
{
    return leftOperand->Value(rng) + rightOperand->Value(rng);
}

TProductExpression::TProductExpression(std::unique_ptr<IExpression>&& left, std::unique_ptr<IExpression>&& right)
    : leftOperand(std::move(left))
    , rightOperand(std::move(right))
{
}

int TProductExpression::Value(IRandomGenerator& rng) const
{
    return leftOperand->Value(rng) * rightOperand->Value(rng);
}

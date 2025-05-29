#include "math_expression.h"

TSumExpression::TSumExpression(std::unique_ptr<IExpression>&& left, std::unique_ptr<IExpression>&& right)
    : leftOperand(std::move(left))
    , rightOperand(std::move(right))
{
}

int TSumExpression::Value(TGameContext& ctx) const
{
    return leftOperand->Value(ctx) + rightOperand->Value(ctx);
}

TProductExpression::TProductExpression(std::unique_ptr<IExpression>&& left, std::unique_ptr<IExpression>&& right)
    : leftOperand(std::move(left))
    , rightOperand(std::move(right))
{
}

int TProductExpression::Value(TGameContext& ctx) const
{
    return leftOperand->Value(ctx) * rightOperand->Value(ctx);
}

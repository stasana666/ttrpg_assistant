#include "number_expression.h"

TNumberExpression::TNumberExpression(int value)
    : value(value)
{
}

int TNumberExpression::Value(TGameContext&) const
{
    return value;
}

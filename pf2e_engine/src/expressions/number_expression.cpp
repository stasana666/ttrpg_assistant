#include "number_expression.h"
#include "random.h"

TNumberExpression::TNumberExpression(int value)
    : value(value)
{
}

int TNumberExpression::Value(IRandomGenerator&) const
{
    return value;
}

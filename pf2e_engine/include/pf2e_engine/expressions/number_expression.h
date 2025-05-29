#pragma once

#include "base_expression.h"

class TNumberExpression final : public IExpression {
public:
    explicit TNumberExpression(int value);

    int Value(TGameContext&) const final;

private:
    int value;
};

#pragma once

#include "base_expression.h"

#include <pf2e_engine/random.h>

class TDiceExpression final : public IExpression {
public:
    explicit TDiceExpression(int size);

    int Value(TGameContext& ctx) const final;

private:
    int size;
};

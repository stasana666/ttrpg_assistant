#pragma once

#include "base_expression.h"

#include <pf2e_engine/random.h>

class TMultiDiceExpression final : public IExpression {
public:
    TMultiDiceExpression(int count, int size);

    int Value(IRandomGenerator& rng) const final;

private:
    int count_;
    int size_;
};

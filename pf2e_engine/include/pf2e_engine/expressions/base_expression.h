#pragma once

#include <pf2e_engine/random.h>

class IExpression {
public:
    IExpression() = default;
    virtual ~IExpression() = default;
    virtual int Value(IRandomGenerator& rng) const = 0;
};

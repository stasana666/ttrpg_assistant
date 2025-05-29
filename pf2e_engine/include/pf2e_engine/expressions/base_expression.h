#pragma once

#include <pf2e_engine/game_context.h>

class IExpression {
public:
    IExpression() = default;
    virtual ~IExpression() = default;
    virtual int Value(TGameContext& ctx) const = 0;
};

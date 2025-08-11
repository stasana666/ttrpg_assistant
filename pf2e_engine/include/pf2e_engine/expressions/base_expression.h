#pragma once

class TGameContext;

class IExpression {
public:
    IExpression() = default;
    virtual ~IExpression() = default;
    virtual int Value(TGameContext& ctx) const = 0;
};

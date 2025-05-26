#pragma once

#include "random.h"

#include <memory>

class IExpression {
public:
    IExpression() = default;
    virtual ~IExpression() = default;
    virtual int Value(IRandomGenerator&) const = 0;
};

class TSumExpression final : public IExpression {
public:
    TSumExpression(std::unique_ptr<IExpression>&& left, std::unique_ptr<IExpression>&& right);

    int Value(IRandomGenerator& rng) const final;

private:
    std::unique_ptr<IExpression> leftOperand;
    std::unique_ptr<IExpression> rightOperand;
};

class TMultiplyExpression final : public IExpression {
public:
    TMultiplyExpression(std::unique_ptr<IExpression>&& left, std::unique_ptr<IExpression>&& right);

    int Value(IRandomGenerator& rng) const final;

private:
    std::unique_ptr<IExpression> leftOperand;
    std::unique_ptr<IExpression> rightOperand;
};

class TDice final : public IExpression {
public:
    explicit TDice(int size);

    int Value(IRandomGenerator& rng) const final;

private:
    int size;
};

class TNumber final : public IExpression {
public:
    explicit TNumber(int value);

    int Value(IRandomGenerator&) const final;
private:
    int value;
};

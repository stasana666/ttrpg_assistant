#pragma once

#include "base_expression.h"
#include <memory>

class TSumExpression final : public IExpression {
public:
    TSumExpression(std::unique_ptr<IExpression>&& left, std::unique_ptr<IExpression>&& right);

    int Value(IRandomGenerator& ctx) const final;

private:
    std::unique_ptr<IExpression> leftOperand;
    std::unique_ptr<IExpression> rightOperand;
};

class TProductExpression final : public IExpression {
public:
    TProductExpression(std::unique_ptr<IExpression>&& left, std::unique_ptr<IExpression>&& right);

    int Value(IRandomGenerator&) const final;

private:
    std::unique_ptr<IExpression> leftOperand;
    std::unique_ptr<IExpression> rightOperand;
};

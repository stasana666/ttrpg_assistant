#pragma once

#include <pf2e_engine/dsl/expression.h>
#include <pf2e_engine/dsl/value.h>

#include <memory>
#include <string>
#include <vector>

class TLiteralExpr final : public IDslExpression {
public:
    explicit TLiteralExpr(TDslValue value);
    TDslValue Evaluate(TEvalContext& ctx) const override;
private:
    TDslValue value_;
};

class TVariableExpr final : public IDslExpression {
public:
    explicit TVariableExpr(std::string name);
    TDslValue Evaluate(TEvalContext& ctx) const override;
private:
    std::string name_;
};

class TPropertyExpr final : public IDslExpression {
public:
    TPropertyExpr(std::unique_ptr<IDslExpression> receiver, std::string property);
    TDslValue Evaluate(TEvalContext& ctx) const override;
private:
    std::unique_ptr<IDslExpression> receiver_;
    std::string property_;
};

class TCallExpr final : public IDslExpression {
public:
    TCallExpr(std::string name, std::vector<std::unique_ptr<IDslExpression>> args);
    TDslValue Evaluate(TEvalContext& ctx) const override;
private:
    std::string name_;
    std::vector<std::unique_ptr<IDslExpression>> args_;
};

enum class EBinaryOp {
    Or, And, Eq, Ne, Lt, Le, Gt, Ge,
};

class TBinaryExpr final : public IDslExpression {
public:
    TBinaryExpr(EBinaryOp op, std::unique_ptr<IDslExpression> lhs, std::unique_ptr<IDslExpression> rhs);
    TDslValue Evaluate(TEvalContext& ctx) const override;
private:
    EBinaryOp op_;
    std::unique_ptr<IDslExpression> lhs_;
    std::unique_ptr<IDslExpression> rhs_;
};

class TUnaryNotExpr final : public IDslExpression {
public:
    explicit TUnaryNotExpr(std::unique_ptr<IDslExpression> inner);
    TDslValue Evaluate(TEvalContext& ctx) const override;
private:
    std::unique_ptr<IDslExpression> inner_;
};

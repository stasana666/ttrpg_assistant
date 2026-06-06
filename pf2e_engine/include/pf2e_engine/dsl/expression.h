#pragma once

#include <pf2e_engine/dsl/value.h>

#include <expr/ast.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

class TBattle;
class TGameObjectRegistry;

struct TEvalContext {
    std::shared_ptr<TGameObjectRegistry> registry;
    TBattle* battle = nullptr;
    std::unordered_map<std::string, TDslValue> scope;
};

class TScopeGuard {
public:
    TScopeGuard(TEvalContext& ctx, std::string name, TDslValue value);
    ~TScopeGuard();
    TScopeGuard(const TScopeGuard&) = delete;
    TScopeGuard& operator=(const TScopeGuard&) = delete;
    void Rebind(TDslValue value);
private:
    TEvalContext& ctx_;
    std::string name_;
    std::optional<TDslValue> saved_;
    bool had_prior_;
};

class TDslExpression {
public:
    explicit TDslExpression(expr::TExprNode root) : root_(std::move(root)) {}
    TDslValue Evaluate(TEvalContext& ctx) const;
private:
    expr::TExprNode root_;
};

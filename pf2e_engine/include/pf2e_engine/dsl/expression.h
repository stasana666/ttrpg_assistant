#pragma once

#include <pf2e_engine/dsl/value.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

class TBattle;
class TGameObjectRegistry;

struct TEvalContext {
    std::shared_ptr<TGameObjectRegistry> registry;
    TBattle* battle = nullptr;
    // Scoped variables shadow registry entries for the duration of their
    // scope. Used by filter/map/foldl to bind $item and $acc per iteration
    // without mutating the registry.
    std::unordered_map<std::string, TDslValue> scope;
};

// RAII helper: pushes `value` into `ctx.scope[name]` for the lifetime of the
// guard, then restores the prior binding (or removes if there was none).
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

class IDslExpression {
public:
    virtual ~IDslExpression() = default;
    virtual TDslValue Evaluate(TEvalContext& ctx) const = 0;
};

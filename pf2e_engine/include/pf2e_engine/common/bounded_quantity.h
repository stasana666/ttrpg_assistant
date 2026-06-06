#pragma once

// A bounded quantity: a current value paired with its maximum. The `.ttrpg`
// language's built-in `BoundedQuantity` field type lowers to this hand-written
// runtime type -- the same arrangement by which `set<Variant>` lowers to the
// hand-written TVariantMap. The generator never emits this type; it only
// references it (FromJson / GetAst), so there is exactly one definition.

#include <pf2e_engine/common/ast/ast_constructable.h>

#include <nlohmann/json_fwd.hpp>

class TGameObjectFactory;

class TBoundedQuantity {
public:
    TBoundedQuantity() = default;
    TBoundedQuantity(int current, int max);
    // Full: current == max == value. Used by computed `= <expr>` initializers.
    explicit TBoundedQuantity(int value);

    int CurrentValue() const { return current_value_; }
    int MaxValue() const { return max_value_; }

    static TBoundedQuantity FromJson(const nlohmann::json& j, const TGameObjectFactory& factory);
    TAstNode GetAst(TAstContext& ctx) const;

private:
    int current_value_{};
    int max_value_{};
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TBoundedQuantity> : std::true_type {};

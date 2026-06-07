#pragma once


#include <pf2e_engine/common/ast/ast_constructable.h>

#include <nlohmann/json_fwd.hpp>

class TGameObjectFactory;

class TBoundedQuantity {
public:
    TBoundedQuantity() = default;
    TBoundedQuantity(int current, int max);
    explicit TBoundedQuantity(int value);

    int CurrentValue() const { return current_value_; }
    int MaxValue() const { return max_value_; }

    void Reduce(int value);
    void Restore(int value);

    static TBoundedQuantity FromJson(const nlohmann::json& j, const TGameObjectFactory& factory);
    TAstNode GetAst(TAstContext& ctx) const;

private:
    int current_value_{};
    int max_value_{};
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TBoundedQuantity> : std::true_type {};

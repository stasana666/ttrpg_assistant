#pragma once


#include <pf2e_engine/common/ast/ast_constructable.h>

#include <nlohmann/json_fwd.hpp>

class TGameObjectFactory;

class TResource {
public:
    TResource() = default;
    explicit TResource(int count);

    int Count() const { return count_; }
    bool Has(int count) const { return count_ >= count; }

    void Reduce(int count);
    void Add(int count);

    static TResource FromJson(const nlohmann::json& j, const TGameObjectFactory& factory);
    TAstNode GetAst(TAstContext& ctx) const;

private:
    int count_{};
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TResource> : std::true_type {};

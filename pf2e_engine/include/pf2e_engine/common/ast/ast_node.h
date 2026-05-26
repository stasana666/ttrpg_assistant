#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

class TAstContext;

class TAstNode {
public:
    static TAstNode MakeNull();
    static TAstNode MakeValue(std::string serialized);
    static TAstNode MakeObject(std::string_view type_name);

    // Deferred reference: stores a raw pointer to a target object whose
    // stable identity may not yet be registered in the context. Resolved to
    // a Value node ("ref:<id>") by Resolve() once the tree is fully built.
    // This lets owning classes register sub-object identities lazily, in any
    // order, without forcing TBattle::GetAst to pre-walk the whole graph.
    static TAstNode MakeDeferredRef(const void* target);

    TAstNode& AddChild(std::string_view label, TAstNode child);

    bool operator ==(const TAstNode& other) const;
    bool operator !=(const TAstNode& other) const;

    std::string PrettyPrint(int indent = 0) const;

    // Returns "" if equal, otherwise a single line describing the first
    // diverging /path and the kind of mismatch.
    std::string DiffWith(const TAstNode& other) const;

    // Walks the tree and replaces every DeferredRef node with a Value node
    // ("ref:<id>") by looking up the target's identity in ctx. Targets that
    // were never registered resolve to "ref:<unresolved>" — a stable marker
    // that makes the omission visible in diffs without crashing the build.
    void Resolve(const TAstContext& ctx);

private:
    TAstNode() = default;

    std::string DiffImpl(const TAstNode& other, const std::string& path) const;
    static std::string KindName(int kind);

    enum class EKind { Null, Value, Object, DeferredRef };
    EKind kind_ = EKind::Null;
    std::string content_;          // serialized value (Value); type_name (Object); empty (DeferredRef until resolved)
    std::vector<std::pair<std::string, TAstNode>> children_;
    const void* deferred_target_ = nullptr;  // valid only when kind_ == DeferredRef
};

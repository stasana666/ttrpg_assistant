#pragma once

#include <pf2e_engine/common/ast/ast_node.h>
#include <pf2e_engine/common/ast/ast_context.h>

#include <memory>
#include <type_traits>

// Marker interface. Classes that participate in AST traversal as OWNED
// subtrees implement this and also specialize TIsAstRecursive<T> to true_type.
class IAstConstructable {
public:
    virtual ~IAstConstructable() = default;
    virtual TAstNode GetAst(TAstContext& ctx) const = 0;
};

// Trait: does T participate in AST traversal as a recursive (owned) subtree?
// Default: false. Each AST-enabled class adds a specialization to true.
// The trait propagates through pointer / smart-pointer wrappers so that
// AddValueField rejects e.g. AddValueField(node, "x", TCreature*).
template <class T>
struct TIsAstRecursive : std::false_type {};

template <class T>
struct TIsAstRecursive<T*> : TIsAstRecursive<std::remove_cv_t<T>> {};

template <class T>
struct TIsAstRecursive<T&> : TIsAstRecursive<std::remove_cv_t<T>> {};

template <class T>
struct TIsAstRecursive<std::shared_ptr<T>> : TIsAstRecursive<std::remove_cv_t<T>> {};

template <class T>
struct TIsAstRecursive<std::unique_ptr<T>> : TIsAstRecursive<std::remove_cv_t<T>> {};

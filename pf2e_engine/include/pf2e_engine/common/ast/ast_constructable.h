#pragma once

#include <pf2e_engine/common/ast/ast_node.h>
#include <pf2e_engine/common/ast/ast_context.h>

#include <memory>
#include <type_traits>

// Trait: does T participate in AST traversal as a recursive (owned) subtree?
// Default: false. Each AST-enabled class adds a specialization to true.
// The trait propagates through pointer / smart-pointer wrappers so that
// AddValueField rejects e.g. AddValueField(node, "x", TCreature*).
//
// AST-enabled classes are recognised structurally (duck typing) — they expose
// a public `TAstNode GetAst(TAstContext&) const`. No marker interface is
// required; inheriting from a base would add a vtable pointer and break the
// strict sizeof-layout checks the system relies on.
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

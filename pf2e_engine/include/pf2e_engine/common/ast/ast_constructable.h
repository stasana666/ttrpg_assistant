#pragma once

#include <pf2e_engine/common/ast/ast_node.h>
#include <pf2e_engine/common/ast/ast_context.h>

#include <memory>
#include <type_traits>

template <class T> struct TIsAstRecursive : std::false_type {};
template <class T> struct TIsAstRecursive<T*> : TIsAstRecursive<std::remove_cv_t<T>> {};
template <class T> struct TIsAstRecursive<T&> : TIsAstRecursive<std::remove_cv_t<T>> {};
template <class T> struct TIsAstRecursive<std::shared_ptr<T>> : TIsAstRecursive<std::remove_cv_t<T>> {};
template <class T> struct TIsAstRecursive<std::unique_ptr<T>> : TIsAstRecursive<std::remove_cv_t<T>> {};

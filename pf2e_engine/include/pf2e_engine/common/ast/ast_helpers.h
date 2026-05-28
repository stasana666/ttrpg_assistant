#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/ast/ast_context.h>
#include <pf2e_engine/common/ast/ast_node.h>
#include <pf2e_engine/common/ast/ast_serialize.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

template <class V>
void AddValueField(TAstNode& parent, std::string_view label, const V& v)
{
    static_assert(!TIsAstRecursive<V>::value,
        "AddValueField: V is AST-recursive. Use AddOwnedObject or AddReference instead.");
    parent.AddChild(label, TAstNode::MakeValue(AstSerialize(v)));
}

// SFINAE on non-pointer prevents the value-ref overload from matching when
// the caller passes a raw pointer (which would otherwise deduce T as Foo*
// and then fail trying to call obj.GetAst(ctx) on the pointer).
template <class T,
          std::enable_if_t<!std::is_pointer_v<T>, int> = 0>
void AddOwnedObject(TAstNode& parent, std::string_view label,
                    const T& obj, TAstContext& ctx)
{
    static_assert(TIsAstRecursive<T>::value,
        "AddOwnedObject: T is not marked AST-recursive.");
    TAstContext::TGuard guard(ctx, static_cast<const void*>(&obj), typeid(T).name());
    parent.AddChild(label, obj.GetAst(ctx));
}

template <class T>
void AddOwnedObject(TAstNode& parent, std::string_view label,
                    const T* obj, TAstContext& ctx)
{
    static_assert(TIsAstRecursive<T>::value,
        "AddOwnedObject: T is not marked AST-recursive.");
    if (obj == nullptr) {
        parent.AddChild(label, TAstNode::MakeNull());
        return;
    }
    AddOwnedObject(parent, label, *obj, ctx);
}

namespace ast_detail {

template <class T>
TAstNode RecurseElement(const T& elem, TAstContext& ctx)
{
    static_assert(TIsAstRecursive<T>::value,
        "Container element is not AST-recursive.");
    TAstContext::TGuard guard(ctx, static_cast<const void*>(&elem), typeid(T).name());
    return elem.GetAst(ctx);
}

template <class T>
TAstNode RecurseElement(const T* elem, TAstContext& ctx)
{
    if (elem == nullptr) {
        return TAstNode::MakeNull();
    }
    return RecurseElement(*elem, ctx);
}

template <class T>
TAstNode RecurseElement(const std::shared_ptr<T>& elem, TAstContext& ctx)
{
    if (!elem) {
        return TAstNode::MakeNull();
    }
    return RecurseElement(*elem, ctx);
}

template <class T>
TAstNode RecurseElement(const std::unique_ptr<T>& elem, TAstContext& ctx)
{
    if (!elem) {
        return TAstNode::MakeNull();
    }
    return RecurseElement(*elem, ctx);
}

}  // namespace ast_detail

template <class Container, class KeyFn>
void AddOwnedContainer(TAstNode& parent, std::string_view label,
                       const Container& c, TAstContext& ctx, KeyFn key_fn)
{
    using Elem = std::remove_cvref_t<decltype(*c.begin())>;

    std::vector<std::pair<std::string, const Elem*>> sorted;
    sorted.reserve(c.size());
    size_t idx = 0;
    for (auto it = c.begin(); it != c.end(); ++it, ++idx) {
        sorted.emplace_back(key_fn(*it, idx), &*it);
    }
    std::stable_sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    TAstNode node = TAstNode::MakeObject("container");
    for (const auto& [key, elem_ptr] : sorted) {
        node.AddChild(key, ast_detail::RecurseElement(*elem_ptr, ctx));
    }
    parent.AddChild(label, std::move(node));
}

inline void AddReference(TAstNode& parent, std::string_view label,
                         const void* identity, TAstContext& ctx)
{
    if (identity == nullptr) {
        parent.AddChild(label, TAstNode::MakeNull());
        return;
    }
    const std::string known = ctx.IdentityOf(identity);
    if (!known.empty()) {
        parent.AddChild(label, TAstNode::MakeValue("ref:" + known));
        return;
    }
    TAstNode* added = parent.AddChild(label, TAstNode::MakeValue(""));
    ctx.RegisterPending(identity, added);
}

inline void AddReference(TAstNode& parent, std::string_view label,
                         const void* identity, std::string_view stable_id)
{
    if (identity == nullptr) {
        parent.AddChild(label, TAstNode::MakeNull());
        return;
    }
    parent.AddChild(label, TAstNode::MakeValue("ref:" + std::string(stable_id)));
}

template <class Container, class PtrFn>
void AddReferenceContainer(TAstNode& parent, std::string_view label,
                           const Container& c, TAstContext& ctx, PtrFn ptr_fn)
{
    TAstNode node = TAstNode::MakeObject("ref_container");
    size_t idx = 0;
    for (auto it = c.begin(); it != c.end(); ++it, ++idx) {
        AddReference(node, std::to_string(idx), ptr_fn(*it, idx), ctx);
    }
    parent.AddChild(label, std::move(node));
}

template <class Fn>
void AddCallbackPlaceholder(TAstNode& parent, std::string_view label, const Fn& fn)
{
    TAstNode node = TAstNode::MakeObject("callback");
    node.AddChild("present", TAstNode::MakeValue(
        std::string("bool:") + (fn ? "1" : "0")));
    if (fn) {
        node.AddChild("target_type",
            TAstNode::MakeValue(std::string("string:") + fn.target_type().name()));
    }
    parent.AddChild(label, std::move(node));
}

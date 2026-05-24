#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/ast/ast_context.h>
#include <pf2e_engine/common/ast/ast_node.h>
#include <pf2e_engine/common/ast/ast_serialize.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

// ------------------------------------------------------------------
// AddValueField: trivially-comparable, value-semantic field.
// Rejected at compile time for any T that is marked AST-recursive.
// ------------------------------------------------------------------
template <class V>
void AddValueField(TAstNode& parent, std::string_view label, const V& v)
{
    static_assert(!TIsAstRecursive<V>::value,
        "AddValueField: V is AST-recursive. Use AddOwnedObject or AddReference instead.");
    parent.AddChild(label, TAstNode::MakeValue(AstSerialize(v)));
}

// ------------------------------------------------------------------
// AddOwnedObject: recurse into an owned IAstConstructable.
// The reference overload is constrained to non-pointer T so that calling
// with `AddOwnedObject(node, "x", some_ptr, ctx)` unambiguously selects
// the pointer overload below. Without this guard, the reference overload
// also matches (deducing T as TFoo*) and then fails inside the body when
// trying obj.GetAst().
// ------------------------------------------------------------------
template <class T,
          std::enable_if_t<!std::is_pointer_v<T>, int> = 0>
void AddOwnedObject(TAstNode& parent, std::string_view label,
                    const T& obj, TAstContext& ctx)
{
    static_assert(TIsAstRecursive<T>::value,
        "AddOwnedObject: T is not marked AST-recursive. Add TIsAstRecursive specialization.");
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

// ------------------------------------------------------------------
// AddOwnedContainer: recurse into every element of an owned container.
// Element type T may be a value, raw pointer, or shared/unique_ptr to an
// IAstConstructable; the wrapping is unpacked transparently here.
// `key_fn(element)` returns a sortable key used to make iteration order
// deterministic for unordered containers. For naturally-ordered containers
// (vector, list, map) `key_fn` may return the index — the relative order
// is preserved either way.
// ------------------------------------------------------------------
namespace ast_detail {

template <class E>
auto Deref(const E& e) -> decltype(*e) { return *e; }

template <class E>
const E& Deref(const E* e) { return *e; }

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

// ------------------------------------------------------------------
// AddReference: non-owning pointer. Records identity (resolved to a stable
// id via the context) without recursing into the referent.
// ------------------------------------------------------------------
inline void AddReference(TAstNode& parent, std::string_view label,
                         const void* identity, const TAstContext& ctx)
{
    if (identity == nullptr) {
        parent.AddChild(label, TAstNode::MakeNull());
        return;
    }
    std::string id = ctx.IdentityOf(identity);
    if (id.empty()) {
        id = "<unregistered>";
    }
    parent.AddChild(label, TAstNode::MakeValue("ref:" + id));
}

// AddReference with an explicit stable id (use when the referent has a
// well-known fixed name, e.g. the battle map).
inline void AddReference(TAstNode& parent, std::string_view label,
                         const void* identity, std::string_view stable_id)
{
    if (identity == nullptr) {
        parent.AddChild(label, TAstNode::MakeNull());
        return;
    }
    parent.AddChild(label, TAstNode::MakeValue("ref:" + std::string(stable_id)));
}

// ------------------------------------------------------------------
// AddReferenceContainer: container of non-owning pointers. id_fn(element)
// returns a stable string id for each entry. Order in the produced AST is
// determined by the iteration order of the container (use ordered
// containers, or pre-sort by key, when calling this).
// ------------------------------------------------------------------
template <class Container, class IdFn>
void AddReferenceContainer(TAstNode& parent, std::string_view label,
                           const Container& c, IdFn id_fn)
{
    TAstNode node = TAstNode::MakeObject("ref_container");
    size_t idx = 0;
    for (auto it = c.begin(); it != c.end(); ++it, ++idx) {
        node.AddChild(std::to_string(idx),
            TAstNode::MakeValue("ref:" + std::string(id_fn(*it, idx))));
    }
    parent.AddChild(label, std::move(node));
}

// ------------------------------------------------------------------
// AddCallbackPlaceholder: documents the presence of a std::function-typed
// field without attempting to compare its captured state. AST equality
// across save/rollback holds only because TTransformator restores the same
// function object verbatim (TRemoveTask::Undo, TRemoveEffect::Undo).
// Do NOT use this for any field that is constructed/replaced outside of
// the rollback-symmetric paths.
// ------------------------------------------------------------------
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

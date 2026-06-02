#pragma once

#include <cstddef>
#include <map>
#include <utility>

// Container for a closed sum type (a generated `variant`) keyed by its
// discriminant. A weapon never carries two traits of the same kind, and rules
// code constantly asks "does this object have trait X, and with what
// parameter?" -- so `set<Variant>` lowers to this rather than std::set<Variant>.
//
// TKind is the variant's kind enum (e.g. EWeaponTraitKind); TVariant is the
// generated wrapper class (e.g. TWeaponTrait), which must expose:
//   - TKind Kind() const
//   - template <class T> const T* TryGet() const
// and each alternative payload struct T must expose `static constexpr TKind Kind`.
//
// std::map iterates in sorted key order, so iteration is deterministic, which
// the AST state-comparison relies on.
template <class TKind, class TVariant>
class TVariantMap {
public:
    void Set(TVariant v) { Items_[v.Kind()] = std::move(v); }
    bool Has(TKind k) const { return Items_.count(k) != 0; }

    // Typed lookup; nullptr if absent or a different alternative.
    template <class T>
    const T* Get() const {
        auto it = Items_.find(T::Kind);
        return it == Items_.end() ? nullptr : it->second.template TryGet<T>();
    }

    auto begin() const { return Items_.begin(); }
    auto end() const { return Items_.end(); }
    bool empty() const { return Items_.empty(); }
    std::size_t size() const { return Items_.size(); }

private:
    std::map<TKind, TVariant> Items_;
};

// Standard overload-set builder for exhaustive std::visit over a variant's
// payload. Adding an alternative to the DSL and regenerating makes any visit
// site built with this fail to compile until the new case is handled.
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

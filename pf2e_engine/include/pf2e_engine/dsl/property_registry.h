#pragma once

#include <pf2e_engine/dsl/expression.h>
#include <pf2e_engine/dsl/value.h>

#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>

// Per-class string→getter map. Dispatch is done by visiting the receiver's
// variant and looking up its TPropertyRegistry<T> specialization.
//
// Getters are stored as type-erased lambdas because getters in this codebase
// have heterogeneous return types (int, const T&, vector<T>, ...) — member
// pointers cannot share a single map type without the same type erasure. The
// `Getter()` helper below wraps a uniform-signature member-function pointer
// when it applies, eliminating the lambda boilerplate at the call site.
template <class T>
class TPropertyRegistry {
public:
    using TGetter = std::function<TDslValue(T*, TEvalContext&)>;

    static TPropertyRegistry& Instance() {
        static TPropertyRegistry inst;
        return inst;
    }

    void Register(std::string name, TGetter getter) {
        getters_.insert({std::move(name), std::move(getter)});
    }

    TDslValue Get(T* obj, const std::string& name, TEvalContext& ctx) const {
        auto it = getters_.find(name);
        if (it == getters_.end()) {
            throw std::runtime_error("dsl: unknown property '" + name + "'");
        }
        return it->second(obj, ctx);
    }

    bool Has(const std::string& name) const {
        return getters_.contains(name);
    }

private:
    std::unordered_map<std::string, TGetter> getters_;
};

// Wraps a uniform-signature getter member-function pointer into the registry
// callable shape. Use when the getter returns something directly convertible
// to TDslValue; otherwise just write the lambda inline.
template <class T, class R, R (T::*M)() const>
auto Getter() {
    return [](T* obj, TEvalContext&) -> TDslValue {
        return TDslValue((obj->*M)());
    };
}

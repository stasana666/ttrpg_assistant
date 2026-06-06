#pragma once

#include <pf2e_engine/dsl/expression.h>
#include <pf2e_engine/dsl/value.h>

#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>

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

template <class T, class R, R (T::*M)() const>
auto Getter() {
    return [](T* obj, TEvalContext&) -> TDslValue {
        return TDslValue((obj->*M)());
    };
}

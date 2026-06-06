#pragma once

#include <cstddef>
#include <map>
#include <utility>

template <class TKind, class TVariant>
class TVariantMap {
public:
    void Set(TVariant v) { Items_[v.Kind()] = std::move(v); }
    void Erase(TKind k) { Items_.erase(k); }
    bool Has(TKind k) const { return Items_.count(k) != 0; }

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

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

#pragma once

#include <compare>
#include <cstddef>
#include <map>
#include <type_traits>
#include <utility>

template <class T>
struct TId {
    std::size_t Value = 0;
    friend auto operator<=>(const TId&, const TId&) = default;
};

template <class T>
class TIdCollection {
public:
    using TStorage = std::map<TId<T>, T>;

    template <bool Const>
    class TEntry {
    public:
        using TIter = std::conditional_t<Const,
            typename TStorage::const_iterator, typename TStorage::iterator>;
        using TWeaponRef = std::conditional_t<Const, const T&, T&>;
        using TWeaponPtr = std::conditional_t<Const, const T*, T*>;

        explicit TEntry(TIter it) : it_(it) {}

        TId<T> Id() const { return it_->first; }
        TWeaponPtr operator->() const { return &it_->second; }
        TWeaponRef operator*() const { return it_->second; }

    private:
        TIter it_;
    };

    template <bool Const>
    class TIterator {
    public:
        using TIter = std::conditional_t<Const,
            typename TStorage::const_iterator, typename TStorage::iterator>;

        explicit TIterator(TIter it) : it_(it) {}

        TEntry<Const> operator*() const { return TEntry<Const>(it_); }
        TIterator& operator++() { ++it_; return *this; }
        bool operator!=(const TIterator& other) const { return it_ != other.it_; }

    private:
        TIter it_;
    };

    TId<T> Add(T value) {
        TId<T> id = next_;
        items_.emplace(id, std::move(value));
        next_ = TId<T>{next_.Value + 1};
        return id;
    }

    void Remove(TId<T> id) { items_.erase(id); }

    T* Find(TId<T> id) {
        auto it = items_.find(id);
        return it == items_.end() ? nullptr : &it->second;
    }

    const T* Find(TId<T> id) const {
        auto it = items_.find(id);
        return it == items_.end() ? nullptr : &it->second;
    }

    bool Contains(TId<T> id) const { return items_.count(id) != 0; }

    std::size_t Size() const { return items_.size(); }

    TIterator<false> begin() { return TIterator<false>(items_.begin()); }
    TIterator<false> end() { return TIterator<false>(items_.end()); }

    TIterator<true> begin() const { return TIterator<true>(items_.begin()); }
    TIterator<true> end() const { return TIterator<true>(items_.end()); }

private:
    TStorage items_;
    TId<T> next_{};
};

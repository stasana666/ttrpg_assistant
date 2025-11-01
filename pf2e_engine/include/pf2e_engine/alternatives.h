#pragma once

#include <cassert>
#include <functional>
#include <string>
#include <vector>

template <class T>
struct TAlternative {
    std::string name;
    T value;
};

template <class T>
class TAlternatives {
public:
    explicit TAlternatives(std::string kind)
        : kind_(kind)
    {
    }

    TAlternatives(std::string kind, std::vector<T>&& values, std::function<std::string(const T&)> descriptor)
        : kind_(kind)
    {
        for (auto&& v : values) {
            alternatives_.push_back(TAlternative<T>{
                .name = descriptor(v),
                .value = std::move(v)
            });
        }
        values.clear();
    }

    TAlternatives& AddAlternative(TAlternative<T> alternative)
    {
        alternatives_.emplace_back(alternative);
        return *this;
    }

    const TAlternative<T>& operator [](size_t index) const
    {
        return alternatives_[index];
    }

    size_t Size() const
    {
        return alternatives_.size();
    }

    bool Empty() const
    {
        return alternatives_.empty();
    }

    std::string_view Kind() const
    {
        return kind_;
    }

private:
    std::string kind_;
    std::vector<TAlternative<T>> alternatives_;
};

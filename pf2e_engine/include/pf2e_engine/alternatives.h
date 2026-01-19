#pragma once

#include <pf2e_engine/position.h>

#include <any>
#include <cassert>
#include <functional>
#include <string>
#include <vector>

enum class EAskingStrategy {
    Console,
    Gui
};

struct TAlternative {
    std::string name;
    std::any value;

    template <class T>
    TAlternative(std::string name, T value)
        : name(std::move(name))
        , value(std::move(value))
    {
    }

    template <class T>
    T Get() const
    {
        return std::any_cast<T>(value);
    }
};

class TAlternatives {
public:
    template <class T>
    static TAlternatives Create(std::string kind)
    {
        return TAlternatives(std::move(kind), GetStrategy<T>());
    }

    template <class T>
    static TAlternatives Create(std::string kind, std::vector<T>&& values, std::function<std::string(const T&)> descriptor)
    {
        TAlternatives result(std::move(kind), GetStrategy<T>());
        for (auto&& v : values) {
            result.alternatives_.emplace_back(descriptor(v), std::move(v));
        }
        return result;
    }

private:
    TAlternatives(std::string kind, EAskingStrategy strategy)
        : kind_(std::move(kind))
        , strategy_(strategy)
    {
    }

public:
    template <class T>
    TAlternatives& AddAlternative(std::string name, T value)
    {
        alternatives_.emplace_back(std::move(name), std::move(value));
        return *this;
    }

    const TAlternative& operator [](size_t index) const
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

    EAskingStrategy GetAskingStrategy() const
    {
        return strategy_;
    }

private:
    template <class T>
    static constexpr EAskingStrategy GetStrategy()
    {
        if constexpr (std::is_same_v<T, TPosition>) {
            return EAskingStrategy::Gui;
        } else {
            return EAskingStrategy::Console;
        }
    }

    std::string kind_;
    EAskingStrategy strategy_;
    std::vector<TAlternative> alternatives_;
};

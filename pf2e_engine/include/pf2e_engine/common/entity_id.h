#pragma once

#include <atomic>
#include <compare>
#include <cstdint>
#include <limits>
#include <stdexcept>

struct TEntityId {
    std::uint64_t Value = 0;
    auto operator<=>(const TEntityId&) const = default;
};

inline std::atomic<std::uint64_t>& EntityIdCounter()
{
    static std::atomic<std::uint64_t> counter{1};
    return counter;
}

inline TEntityId NewEntityId()
{
    auto& counter = EntityIdCounter();
    auto value = counter.load(std::memory_order_relaxed);
    do {
        if (value == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error("entity identity space exhausted");
        }
    } while (!counter.compare_exchange_weak(value, value + 1, std::memory_order_relaxed));
    return {value};
}

inline TEntityId RestoreEntityId(std::uint64_t value)
{
    if (value == 0 || value == std::numeric_limits<std::uint64_t>::max()) {
        throw std::invalid_argument("invalid entity identity");
    }
    auto& counter = EntityIdCounter();
    auto next = counter.load(std::memory_order_relaxed);
    while (next <= value && !counter.compare_exchange_weak(next, value + 1, std::memory_order_relaxed)) {}
    return {value};
}

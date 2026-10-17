#pragma once

#include <compare>
#include <cstdint>

struct TEntityId {
    std::uint64_t Value = 0;
    auto operator<=>(const TEntityId&) const = default;
};

TEntityId NewEntityId();
std::uint64_t PeekNextEntityId();

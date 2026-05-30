#pragma once

#include <string>

struct TPosition {
    int x;
    int y;

    bool operator ==(const TPosition& other) const = default;
};

inline std::string AstSerialize(const TPosition& p)
{
    return "TPosition{" + std::to_string(p.x) + "," + std::to_string(p.y) + "}";
}

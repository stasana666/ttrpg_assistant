#pragma once

struct TPosition {
    int x;
    int y;

    bool operator ==(const TPosition& other) const = default;
};

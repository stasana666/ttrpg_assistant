#pragma once

#include <limits>

class TGameObjectFactory;

class TArmor {
public:
    int AcBonus() const;
    int DexCap() const;

private:
    friend TGameObjectFactory;

    int ac_bonus_{0};
    int dex_cap_{std::numeric_limits<int>::max()};
};

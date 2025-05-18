#pragma once

class TArmour {
public:
    TArmour(int ac_bonus, int dex_cap);

    int AcBonus() const;
    int DexCap() const;

private:
    int ac_bonus;
    int dex_cap;
};

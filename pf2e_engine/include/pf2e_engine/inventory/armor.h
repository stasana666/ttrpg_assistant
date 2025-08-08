#pragma once

class TArmor {
public:
    TArmor(int ac_bonus, int dex_cap);

    static const TArmor& GetDefault();

    int AcBonus() const;
    int DexCap() const;

private:
    int ac_bonus_;
    int dex_cap_;
};

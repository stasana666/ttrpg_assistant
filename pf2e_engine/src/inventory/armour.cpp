#include "armour.h"

TArmour::TArmour(int ac_bonus, int dex_cap)
    : ac_bonus(ac_bonus)
    , dex_cap(dex_cap)
{
}

int TArmour::AcBonus() const {
    return ac_bonus;
}

int TArmour::DexCap() const {
    return dex_cap;
}

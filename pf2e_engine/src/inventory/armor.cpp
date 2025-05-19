#include "armor.h"
#include <limits>

TArmor::TArmor(int ac_bonus, int dex_cap)
    : ac_bonus(ac_bonus)
    , dex_cap(dex_cap)
{
}

const TArmor& TArmor::GetDefault()
{
    static TArmor defaultArmour(0, std::numeric_limits<int>::max());
    return defaultArmour;
}

int TArmor::AcBonus() const {
    return ac_bonus;
}

int TArmor::DexCap() const {
    return dex_cap;
}

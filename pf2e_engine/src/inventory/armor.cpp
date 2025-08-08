#include "armor.h"
#include <limits>

TArmor::TArmor(int ac_bonus, int dex_cap)
    : ac_bonus_(ac_bonus)
    , dex_cap_(dex_cap)
{
}

const TArmor& TArmor::GetDefault()
{
    static TArmor default_armour(0, std::numeric_limits<int>::max());
    return default_armour;
}

int TArmor::AcBonus() const {
    return ac_bonus_;
}

int TArmor::DexCap() const {
    return dex_cap_;
}

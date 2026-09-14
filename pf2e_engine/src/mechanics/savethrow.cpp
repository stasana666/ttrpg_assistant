#include <pf2e_engine/mechanics/savethrows.h>

#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/inventory/statistics.h>

ESavethrow SavethrowFromString(std::string str_savethrow)
{
    return ESavethrowFromString(str_savethrow);
}

ECharacteristic BindedCharacteristic(ESavethrow savethrow)
{
    return TStatisticRules::Create({}).SaveAttribute(savethrow);
}

#include <pf2e_engine/mechanics/savethrows.h>

#include <pf2e_engine/mechanics/characteristics.h>

#include <stdexcept>

ESavethrow SavethrowFromString(std::string str_savethrow)
{
    return ESavethrowFromString(str_savethrow);
}

ECharacteristic BindedCharacteristic(ESavethrow savethrow)
{
    switch (savethrow) {
        case ESavethrow::Fortitude:
            return ECharacteristic::Constitution;
        case ESavethrow::Reflex:
            return ECharacteristic::Dexterity;
        case ESavethrow::Will:
            return ECharacteristic::Wisdom;
    }
    throw std::invalid_argument("incorrect value of ESavethrow");
}

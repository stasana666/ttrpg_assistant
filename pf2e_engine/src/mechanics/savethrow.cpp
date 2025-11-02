#include <pf2e_engine/mechanics/savethrows.h>

#include <pf2e_engine/mechanics/characteristics.h>

#include <stdexcept>

std::string ToString(ESavethrow savethrow)
{
    switch (savethrow) {
        case ESavethrow::Fortitude:
            return "Fortitude";
        case ESavethrow::Reflex:
            return "Reflex";
        case ESavethrow::Will:
            return "Will";
    }
    throw std::invalid_argument("incorrect value of ESavethrow");
}

ESavethrow SavethrowFromString(std::string str_savethrow)
{
    for (auto savethrow : {ESavethrow::Fortitude, ESavethrow::Reflex, ESavethrow::Will}) {
        if (ToString(savethrow) == str_savethrow) {
            return savethrow;
        }
    }
    throw std::invalid_argument("unknown ESavethrow: \"" + str_savethrow + "\"");
}

constexpr ECharacteristic BindedCharacteristic(ESavethrow savethrow)
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

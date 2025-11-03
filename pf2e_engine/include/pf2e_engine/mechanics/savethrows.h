#pragma once

#include <pf2e_engine/mechanics/characteristics.h>

#include <string>

enum class ESavethrow {
    Fortitude,
    Will,
    Reflex,
};

std::string ToString(ESavethrow savethrow);
ESavethrow SavethrowFromString(std::string str_savethrow);

ECharacteristic BindedCharacteristic(ESavethrow savethrow);

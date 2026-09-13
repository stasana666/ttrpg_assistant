#pragma once

#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/inventory/saving_throws.h>

#include <string>

ESavethrow SavethrowFromString(std::string str_savethrow);

ECharacteristic BindedCharacteristic(ESavethrow savethrow);

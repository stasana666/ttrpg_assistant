#pragma once

#include <string>

enum class ECreatureSize {
    Tiny,
    Small,
    Medium,
    Large,
    Huge,
    Gargantuan,
    COUNT,
};

std::string ToString(ECreatureSize);
ECreatureSize CreatureSizeFromString(std::string);

#include <pf2e_engine/creature_size.h>

#include <stdexcept>

std::string ToString(ECreatureSize size)
{
    switch (size) {
        case ECreatureSize::Tiny:       return "Tiny";
        case ECreatureSize::Small:      return "Small";
        case ECreatureSize::Medium:     return "Medium";
        case ECreatureSize::Large:      return "Large";
        case ECreatureSize::Huge:       return "Huge";
        case ECreatureSize::Gargantuan: return "Gargantuan";
        case ECreatureSize::COUNT:
            throw std::invalid_argument("COUNT is not valid ECreatureSize");
    }
    throw std::invalid_argument("incorrect value of ECreatureSize");
}

ECreatureSize CreatureSizeFromString(std::string size)
{
    for (size_t i = 0; i < static_cast<size_t>(ECreatureSize::COUNT); ++i) {
        if (ToString(static_cast<ECreatureSize>(i)) == size) {
            return static_cast<ECreatureSize>(i);
        }
    }
    throw std::invalid_argument("unknown ECreatureSize: \"" + size + "\"");
}

#include <characteristics.h>
#include <stdexcept>

TCharacteristic::TCharacteristic(int value)
    : value_(value)
{
}

int TCharacteristic::GetValue() const
{
    return value_;
}

int TCharacteristic::GetMod() const
{
    return value_ / 2 - 5;
}

void TCharacteristic::Set(int value)
{
    value_ = value;
    NotifyAll(*this);
}

TCharacteristicSet::TCharacteristicSet(std::array<int, kCharacteristicCount> values)
    : stats_{
        TCharacteristic(values[0]),
        TCharacteristic(values[1]),
        TCharacteristic(values[2]),
        TCharacteristic(values[3]),
        TCharacteristic(values[4]),
        TCharacteristic(values[5]),
    }
{
}

TCharacteristic& TCharacteristicSet::operator[](ECharacteristic name)
{
    switch (name) {
        case ECharacteristic::Strength:     return stats_[0];
        case ECharacteristic::Dexterity:    return stats_[1];
        case ECharacteristic::Constitution: return stats_[2];
        case ECharacteristic::Intelligence: return stats_[3];
        case ECharacteristic::Wisdom:       return stats_[4];
        case ECharacteristic::Charisma:     return stats_[5];
    }
    throw std::logic_error("unreachable");
}

const TCharacteristic& TCharacteristicSet::operator[](ECharacteristic name) const
{
    switch (name) {
        case ECharacteristic::Strength:     return stats_[0];
        case ECharacteristic::Dexterity:    return stats_[1];
        case ECharacteristic::Constitution: return stats_[2];
        case ECharacteristic::Intelligence: return stats_[3];
        case ECharacteristic::Wisdom:       return stats_[4];
        case ECharacteristic::Charisma:     return stats_[5];
    }
    throw std::logic_error("unreachable");
}

////////////////////////////////////////////////////////////////////

ECharacteristic CharacteristicFromString(std::string_view sv)
{
    for (size_t i = 0; i < TCharacteristicSet::kCharacteristicCount; ++i) {
        if (sv == ToString(static_cast<ECharacteristic>(i))) {
            return static_cast<ECharacteristic>(i);
        }
    }
    throw std::logic_error("unknown characteristic name");
}

std::string ToString(ECharacteristic name)
{
    switch (name) {
    case ECharacteristic::Strength:     return "Strength";
    case ECharacteristic::Dexterity:    return "Dexterity";
    case ECharacteristic::Constitution: return "Constitution";
    case ECharacteristic::Intelligence: return "Intelligence";
    case ECharacteristic::Wisdom:       return "Wisdom";
    case ECharacteristic::Charisma:     return "Charisma";
    }
    throw std::logic_error("unreachable");
}

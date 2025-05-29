#include <characteristics.h>
#include <stdexcept>

TCharacteristic::TCharacteristic(int value)
    : value(value)
{
}

int TCharacteristic::GetValue() const
{
    return value;
}

int TCharacteristic::GetMod() const
{
    return value / 2 - 5;
}

void TCharacteristic::Set(int value_)
{
    value = value_;
    NotifyAll(*this);
}

TCharacteristicSet::TCharacteristicSet(std::array<int, kCharacteristicCount> values)
    : stats{
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
        case ECharacteristic::Strength:     return stats[0];
        case ECharacteristic::Dexterity:    return stats[1];
        case ECharacteristic::Constitution: return stats[2];
        case ECharacteristic::Intelligence: return stats[3];
        case ECharacteristic::Wisdom:       return stats[4];
        case ECharacteristic::Charisma:     return stats[5];
    }
    throw std::logic_error("unreachable");
}

const TCharacteristic& TCharacteristicSet::operator[](ECharacteristic name) const
{
    switch (name) {
        case ECharacteristic::Strength:     return stats[0];
        case ECharacteristic::Dexterity:    return stats[1];
        case ECharacteristic::Constitution: return stats[2];
        case ECharacteristic::Intelligence: return stats[3];
        case ECharacteristic::Wisdom:       return stats[4];
        case ECharacteristic::Charisma:     return stats[5];
    }
    throw std::logic_error("unreachable");
}

////////////////////////////////////////////////////////////////////

ECharacteristic CharacteristicFromString(std::string_view sv)
{
    if (sv == "strength")       return ECharacteristic::Strength;
    if (sv == "dexterity")      return ECharacteristic::Dexterity;
    if (sv == "constitution")   return ECharacteristic::Constitution;
    if (sv == "intelligence")   return ECharacteristic::Intelligence;
    if (sv == "wisdom")         return ECharacteristic::Wisdom;
    if (sv == "charisma")       return ECharacteristic::Charisma;
    throw std::logic_error("unknown characteristic name");
}

std::string ToString(ECharacteristic name)
{
    switch (name) {
    case ECharacteristic::Strength:     return "strength";
    case ECharacteristic::Dexterity:    return "dexterity";
    case ECharacteristic::Constitution: return "constitution";
    case ECharacteristic::Intelligence: return "intelligence";
    case ECharacteristic::Wisdom:       return "wisdom";
    case ECharacteristic::Charisma:     return "charisma";
    }
    throw std::logic_error("unreachable");
}

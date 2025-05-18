#pragma once

#include <array>
#include <string_view>
#include <string>

enum class ECharacteristic {
    Strength,
    Dexterity,
    Constitution,
    Intelligence,
    Wisdom,
    Charisma,
};

class TCharacteristic {
public:
    TCharacteristic(int value);

    int GetMod() const;
    int GetValue() const;
    void Set(int value_);

private:
    int value;
};

class TCharacteristicSet {
public:
    static constexpr size_t kCharacteristicCount = 6;

public:
    TCharacteristicSet(std::array<int, kCharacteristicCount> values);

    TCharacteristic& operator[](ECharacteristic stat);
    const TCharacteristic& operator[](ECharacteristic stat) const;
private:
    std::array<TCharacteristic, kCharacteristicCount> stats;
};

ECharacteristic CharacteristicFromString(std::string_view);
std::string ToString(ECharacteristic);

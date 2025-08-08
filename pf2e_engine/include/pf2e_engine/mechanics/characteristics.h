#pragma once

#include <pf2e_engine/common/observable.h>

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

class TCharacteristic final : public TObservable<const TCharacteristic&> {
public:
    explicit TCharacteristic(int value);

    int GetMod() const;
    int GetValue() const;
    void Set(int value);

private:
    int value_;
};

class TCharacteristicSet {
public:
    static constexpr size_t kCharacteristicCount = 6;

public:
    explicit TCharacteristicSet(std::array<int, kCharacteristicCount> values);

    TCharacteristic& operator[](ECharacteristic stat);
    const TCharacteristic& operator[](ECharacteristic stat) const;
private:
    std::array<TCharacteristic, kCharacteristicCount> stats_;
};

ECharacteristic CharacteristicFromString(std::string_view);
std::string ToString(ECharacteristic);

#include <gtest/gtest.h>

#include <characteristics.h>

TEST(CharacteristicsTest, FromString) {
    std::unordered_map<std::string, ECharacteristic> table{
        {"strength", ECharacteristic::Strength},
        {"dexterity", ECharacteristic::Dexterity},
        {"constitution", ECharacteristic::Constitution},
        {"intelligence", ECharacteristic::Intelligence},
        {"wisdom", ECharacteristic::Wisdom},
        {"charisma", ECharacteristic::Charisma},
    };

    for (const auto& [key, value] : table) {
        EXPECT_EQ(CharacteristicFromString(key), value);
    }
}

TEST(CharacteristicsTest, ToString) {
    std::unordered_map<ECharacteristic, std::string> table{
        {ECharacteristic::Strength, "strength"},
        {ECharacteristic::Dexterity, "dexterity"},
        {ECharacteristic::Constitution, "constitution"},
        {ECharacteristic::Intelligence, "intelligence"},
        {ECharacteristic::Wisdom, "wisdom"},
        {ECharacteristic::Charisma, "charisma"},
    };

    for (const auto& [key, value] : table) {
        EXPECT_EQ(ToString(key), value);
    }
}

TEST(CharacteristicsTest, ValidCharacteristic) {
    TCharacteristic characteristic(10);

    EXPECT_EQ(characteristic.GetValue(), 10);
    EXPECT_EQ(characteristic.GetMod(), 0);
}

TEST(CharacteristicsTest, CharacteristicSet) {
    TCharacteristicSet stats({10, 10, 10, 10, 10, 10});

    std::unordered_map<ECharacteristic, std::pair<int, int>> table{
        {ECharacteristic::Strength, {7, -2}},
        {ECharacteristic::Dexterity, {20, 5}},
        {ECharacteristic::Constitution, {12, 1}},
        {ECharacteristic::Intelligence, {12, 1}},
        {ECharacteristic::Wisdom, {7, -2}},
        {ECharacteristic::Charisma, {18, 4}},
    };

    for (const auto& [key, value] : table) {
        EXPECT_EQ(stats[key].GetValue(), 10);
        EXPECT_EQ(stats[key].GetMod(), 0);
        stats[key].Set(value.first);
        EXPECT_EQ(stats[key].GetValue(), value.first);
        EXPECT_EQ(stats[key].GetMod(), value.second);
    }
}

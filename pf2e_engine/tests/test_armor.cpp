#include <gtest/gtest.h>

#include <pf2e_engine/creature.h>

std::unordered_map<std::string, TArmor> armors{
    {"Unarmored", TArmor(0, std::numeric_limits<int>::max())},
    {"Leather", TArmor(1, 4)},
    {"Full Plate", TArmor(6, 0)},
};

TEST(ArmorClassTest, ArmorClass) {
    TArmorSlot slot;
    TCharacteristic dex(18);

    TArmorClass ac(slot, dex);
    EXPECT_EQ(ac.GetAc(), 14);

    slot.Set(&armors.at("Unarmored"));
    EXPECT_EQ(ac.GetAc(), 14);

    dex.Set(30);
    EXPECT_EQ(ac.GetAc(), 20);

    slot.Set(&armors.at("Leather"));
    EXPECT_EQ(ac.GetAc(), 15);

    dex.Set(14);
    EXPECT_EQ(ac.GetAc(), 13);

    slot.Set(&armors.at("Full Plate"));
    EXPECT_EQ(ac.GetAc(), 16);

    dex.Set(30);
    EXPECT_EQ(ac.GetAc(), 16);

    dex.Set(8);
    EXPECT_EQ(ac.GetAc(), 15);
}

#include <gtest/gtest.h>

#include <pf2e_engine/creature.h>

std::unordered_map<std::string, TArmour> armours{
    {"Unarmored", TArmour(0, std::numeric_limits<int>::max())},
    {"Leather", TArmour(1, 4)},
    {"Full Plate", TArmour(6, 0)},
};

TEST(CreatureTest, ArmourClass) {
    for (const auto& [armour_name, armour] : armours) {
        for (int dex = 1; dex < 30; ++dex) {
            TCharacteristicSet stats({0, dex, 0, 0, 0, 0});
            TCreature creature(stats, armour, THitPoints(0));

            //EXPECT_EQ(creature.GetAc(), armour.AcBonus() + std::min(armour.DexCap(), stats[ECharacteristic::Dexterity].GetMod());
        }
    }
}

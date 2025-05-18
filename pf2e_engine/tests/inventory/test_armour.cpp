#include <gtest/gtest.h>

#include "armour.h"

TEST(ArmourTest, SimpleArmour) {
    TArmour armour(18, 1);

    EXPECT_EQ(armour.AcBonus(), 18);
    EXPECT_EQ(armour.DexCap(), 1);
}

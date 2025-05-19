#include <gtest/gtest.h>

#include "dice.h"
#include "random.h"

TEST(RandomTest, EqualSeed) {
    constexpr int N = 10;
    TRandomGenerator rng1(42);
    TRandomGenerator rng2(42);

    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(rng1.RollDice(20), rng2.RollDice(20));
    }
}

TEST(RandomTest, ChiSquared) {
    TRandomGenerator rng(42);
    constexpr int numRolls = 10000;
    constexpr int diceSize = 20;
    double expected = static_cast<double>(numRolls) / diceSize;

    std::vector<int> counts(diceSize, 0);
    
    for (int i = 0; i < numRolls; ++i) {
        ++counts[rng.RollDice(diceSize) - 1];
    }

    double chiSquared = 0.0;
    for (int i = 0; i < diceSize; ++i) {
        double observed = counts[i];
        chiSquared += (observed - expected) * (observed - expected) / expected;
    }

    double criticalValue = 30.1; // для p_value 0.05 и 20 гранного кубика
    EXPECT_LT(chiSquared, criticalValue);
}

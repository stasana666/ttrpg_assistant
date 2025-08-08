#include <gtest/gtest.h>

#include <pf2e_engine/random.h>

TEST(RandomTest, EqualSeed) {
    constexpr int kN = 10;
    TRandomGenerator rng1(42);
    TRandomGenerator rng2(42);

    for (int i = 0; i < kN; ++i) {
        EXPECT_EQ(rng1.RollDice(20), rng2.RollDice(20));
    }
}

TEST(RandomTest, ChiSquared) {
    TRandomGenerator rng(42);
    constexpr int kNumRolls = 10000;
    constexpr int kDiceSize = 20;
    double expected = static_cast<double>(kNumRolls) / kDiceSize;

    std::vector<int> counts(kDiceSize, 0);
    
    for (int i = 0; i < kNumRolls; ++i) {
        ++counts[rng.RollDice(kDiceSize) - 1];
    }

    double chi_squared = 0.0;
    for (int i = 0; i < kDiceSize; ++i) {
        double observed = counts[i];
        chi_squared += (observed - expected) * (observed - expected) / expected;
    }

    double critical_value = 30.1; // для p_value 0.05 и 20 гранного кубика
    EXPECT_LT(chi_squared, critical_value);
}

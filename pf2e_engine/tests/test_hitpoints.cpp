#include <gtest/gtest.h>

#include <pf2e_engine/hitpoints.h>

TEST(HitPointsTest, ReduceAndRestore) {
    THitPoints hp(10);
    EXPECT_EQ(hp.GetCurrentHp(), 10);

    hp.ReduceHp(5);
    EXPECT_EQ(hp.GetCurrentHp(), 5);

    hp.RestoreHp(2);
    EXPECT_EQ(hp.GetCurrentHp(), 7);

    hp.ReduceHp(100);
    EXPECT_EQ(hp.GetCurrentHp(), 0);

    hp.RestoreHp(1);
    EXPECT_EQ(hp.GetCurrentHp(), 1);

    hp.RestoreHp(100);
    EXPECT_EQ(hp.GetCurrentHp(), 10);
}

TEST(HitPointsTest, TemporaryHp) {
    THitPoints hp(10);

    hp.SetTemporaryHp(5);
    EXPECT_EQ(hp.GetCurrentHp(), 15);

    hp.RestoreHp(100);
    EXPECT_EQ(hp.GetCurrentHp(), 15);

    hp.ReduceHp(10);
    EXPECT_EQ(hp.GetCurrentHp(), 5);

    hp.RestoreHp(10);
    EXPECT_EQ(hp.GetCurrentHp(), 10);
}

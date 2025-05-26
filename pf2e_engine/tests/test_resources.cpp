#include <gtest/gtest.h>

#include <pf2e_engine/resources.h>

TEST(ResourcesTest, ResourcesRegister) {
    TResourceManager manager;
    auto id1 = manager.Register("action");
    auto id2 = manager.Register("spell_slot");

    EXPECT_FALSE(id1 == id2);
    EXPECT_EQ(id1, manager.Register("action"));
    EXPECT_EQ(id2, manager.Register("spell_slot"));

    EXPECT_EQ("action", manager.Name(id1));
    EXPECT_EQ("spell_slot", manager.Name(id2));
}

TEST(ResourcesTest, ResourcePool) {
    TResourceManager manager;
    auto hand = manager.Register("hand");
    auto bucklerHand = manager.Register("hand_with_buckler");

    TResourcePool pool;

    EXPECT_EQ(pool.Count(hand), 0);
    EXPECT_EQ(pool.Count(bucklerHand), 0);

    pool.Add(hand, 2);

    EXPECT_EQ(pool.Count(hand), 2);
    EXPECT_EQ(pool.Count(bucklerHand), 0);

    EXPECT_TRUE(pool.HasResource(hand, 1));
    EXPECT_FALSE(pool.HasResource(bucklerHand, 1));

    pool.Reduce(hand, 1);

    EXPECT_EQ(pool.Count(hand), 1);
    EXPECT_EQ(pool.Count(bucklerHand), 0);

    pool.Add(bucklerHand, 1);

    EXPECT_EQ(pool.Count(hand), 1);
    EXPECT_EQ(pool.Count(bucklerHand), 1);

    EXPECT_TRUE(pool.HasResource(hand, 1));
    EXPECT_TRUE(pool.HasResource(bucklerHand, 1));
}

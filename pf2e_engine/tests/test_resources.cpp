#include <gtest/gtest.h>

#include <pf2e_engine/resources.h>

TEST(ResourcesTest, ResourcesRegister) {
    TResourceIdManager manager;
    auto id1 = manager.Register("action");
    auto id2 = manager.Register("spell_slot");
    EXPECT_FALSE(id1 == id2);
    EXPECT_EQ(id1, manager.Register("action"));
    EXPECT_EQ(id2, manager.Register("spell_slot"));

    EXPECT_EQ("action", manager.Name(id1));
    EXPECT_EQ("spell_slot", manager.Name(id2));
}

TEST(ResourcesTest, ResourcePool) {
    TResourceIdManager manager;
    auto hand = manager.Register("hand");
    auto buckler_hand = manager.Register("hand_with_buckler");

    TResourcePool pool;

    EXPECT_EQ(pool.Count(hand), 0);
    EXPECT_EQ(pool.Count(buckler_hand), 0);

    pool.Add(hand, 2);

    EXPECT_EQ(pool.Count(hand), 2);
    EXPECT_EQ(pool.Count(buckler_hand), 0);

    EXPECT_TRUE(pool.HasResource(hand, 1));
    EXPECT_FALSE(pool.HasResource(buckler_hand, 1));

    pool.Reduce(hand, 1);

    EXPECT_EQ(pool.Count(hand), 1);
    EXPECT_EQ(pool.Count(buckler_hand), 0);

    pool.Add(buckler_hand, 1);

    EXPECT_EQ(pool.Count(hand), 1);
    EXPECT_EQ(pool.Count(buckler_hand), 1);

    EXPECT_TRUE(pool.HasResource(hand, 1));
    EXPECT_TRUE(pool.HasResource(buckler_hand, 1));
}

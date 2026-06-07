#include <gtest/gtest.h>

#include <pf2e_engine/common/resource.h>

TEST(ResourceTest, CountAndHas) {
    TResource resource(3);
    EXPECT_EQ(resource.Count(), 3);
    EXPECT_TRUE(resource.Has(3));
    EXPECT_TRUE(resource.Has(1));
    EXPECT_FALSE(resource.Has(4));
}

TEST(ResourceTest, DefaultIsZero) {
    TResource resource;
    EXPECT_EQ(resource.Count(), 0);
    EXPECT_FALSE(resource.Has(1));
}

TEST(ResourceTest, Add) {
    TResource resource;
    resource.Add(2);
    EXPECT_EQ(resource.Count(), 2);
    resource.Add(3);
    EXPECT_EQ(resource.Count(), 5);
}

TEST(ResourceTest, ReduceClampsAtZero) {
    TResource resource(2);
    resource.Reduce(1);
    EXPECT_EQ(resource.Count(), 1);
    resource.Reduce(5);
    EXPECT_EQ(resource.Count(), 0);
}

#include <gtest/gtest.h>

#include <pf2e_engine/common/bounded_quantity.h>

#include <stdexcept>

TEST(BoundedQuantityTest, ReduceAndRestore) {
    TBoundedQuantity hp(10);
    EXPECT_EQ(hp.CurrentValue(), 10);
    EXPECT_EQ(hp.MaxValue(), 10);

    hp.Reduce(5);
    EXPECT_EQ(hp.CurrentValue(), 5);

    hp.Restore(2);
    EXPECT_EQ(hp.CurrentValue(), 7);

    hp.Reduce(100);
    EXPECT_EQ(hp.CurrentValue(), 0);

    hp.Restore(1);
    EXPECT_EQ(hp.CurrentValue(), 1);

    hp.Restore(100);
    EXPECT_EQ(hp.CurrentValue(), 10);
}

TEST(BoundedQuantityTest, Exceptions) {
    TBoundedQuantity hp(10);

    EXPECT_THROW(hp.Reduce(-10), std::logic_error);
    EXPECT_EQ(hp.CurrentValue(), 10);

    EXPECT_THROW(hp.Restore(-10), std::logic_error);
    EXPECT_EQ(hp.CurrentValue(), 10);
}

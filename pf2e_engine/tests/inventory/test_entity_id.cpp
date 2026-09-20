#include <pf2e_engine/common/entity_id.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <latch>
#include <thread>
#include <vector>

TEST(EntityIdTest, RestoringAFutureIdentityAdvancesAllocation)
{
    const auto restored_value = PeekNextEntityId() + 32;
    EXPECT_EQ(RestoreEntityId(restored_value).Value, restored_value);
    EXPECT_GT(NewEntityId().Value, restored_value);
}

TEST(EntityIdTest, RestoringAnOlderIdentityDoesNotRewindAllocation)
{
    const auto older = NewEntityId();
    const auto newer = NewEntityId();
    const auto next = PeekNextEntityId();

    EXPECT_EQ(RestoreEntityId(older.Value), older);
    EXPECT_EQ(PeekNextEntityId(), next);
    EXPECT_GT(NewEntityId().Value, newer.Value);
}

TEST(EntityIdTest, ConcurrentAllocationRemainsUniqueDuringRestore)
{
    constexpr std::size_t kThreadCount = 4;
    constexpr std::size_t kIdsPerThread = 64;
    std::array<std::array<std::uint64_t, kIdsPerThread>, kThreadCount> allocated{};
    std::array<std::uint64_t, kIdsPerThread> observed{};
    std::latch start{1};
    {
        std::vector<std::jthread> workers;
        workers.reserve(kThreadCount + 1);
        for (std::size_t index = 0; index < kThreadCount; ++index) {
            workers.emplace_back([&, index] {
                start.wait();
                for (auto& value : allocated[index]) {
                    value = NewEntityId().Value;
                }
            });
        }
        workers.emplace_back([&] {
            start.wait();
            for (auto& next : observed) {
                next = PeekNextEntityId();
                RestoreEntityId(next + 2);
            }
        });
        start.count_down();
    }

    std::vector<std::uint64_t> values;
    values.reserve(kThreadCount * kIdsPerThread);
    for (const auto& thread_values : allocated) {
        values.insert(values.end(), thread_values.begin(), thread_values.end());
    }
    std::ranges::sort(values);
    EXPECT_NE(values.front(), 0u);
    EXPECT_EQ(std::adjacent_find(values.begin(), values.end()), values.end());
    EXPECT_TRUE(std::ranges::is_sorted(observed));
    EXPECT_GT(PeekNextEntityId(), values.back());
    EXPECT_GT(PeekNextEntityId(), observed.back());
}

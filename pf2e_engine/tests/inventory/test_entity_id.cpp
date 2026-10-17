#include <pf2e_engine/common/entity_id.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <latch>
#include <thread>
#include <vector>

TEST(EntityIdTest, ConcurrentAllocationRemainsUnique)
{
    constexpr std::size_t kThreadCount = 4;
    constexpr std::size_t kIdsPerThread = 64;
    std::array<std::array<std::uint64_t, kIdsPerThread>, kThreadCount> allocated{};
    std::latch start{1};
    {
        std::vector<std::jthread> workers;
        workers.reserve(kThreadCount);
        for (std::size_t index = 0; index < kThreadCount; ++index) {
            workers.emplace_back([&, index] {
                start.wait();
                for (auto& value : allocated[index]) {
                    value = NewEntityId().Value;
                }
            });
        }
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
    EXPECT_GT(PeekNextEntityId(), values.back());
}

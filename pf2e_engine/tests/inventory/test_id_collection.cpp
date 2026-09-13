#include <pf2e_engine/common/id_collection.h>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <string>

TEST(IdCollectionTest, RestorePreservesExplicitIdsAndSupportsUnorderedEntries)
{
    TIdCollection<std::string> items;
    items.RestoreEntry(TId<std::string>{7}, "seven");
    items.RestoreEntry(TId<std::string>{2}, "two");

    ASSERT_NE(items.Find(TId<std::string>{7}), nullptr);
    EXPECT_EQ(*items.Find(TId<std::string>{7}), "seven");
    EXPECT_EQ(*items.Find(TId<std::string>{2}), "two");
    EXPECT_EQ(items.NextId().Value, 8u);
    EXPECT_EQ(items.Add("eight").Value, 8u);
}

TEST(IdCollectionTest, RestorePreservesHolesAndRemovedHighestId)
{
    TIdCollection<int> original;
    original.Add(10);
    const auto removed_middle = original.Add(20);
    original.Add(30);
    const auto removed_last = original.Add(40);
    original.Remove(removed_middle);
    original.Remove(removed_last);

    TIdCollection<int> restored;
    for (auto entry : original) {
        restored.RestoreEntry(entry.Id(), *entry);
    }
    restored.RestoreNextId(original.NextId());

    EXPECT_EQ(restored.Size(), 2u);
    EXPECT_FALSE(restored.Contains(removed_middle));
    EXPECT_FALSE(restored.Contains(removed_last));
    EXPECT_EQ(*restored.Find(TId<int>{2}), 30);
    EXPECT_EQ(restored.Add(50).Value, 4u);
}

TEST(IdCollectionTest, EmptyRestoreKeepsAllocationHistory)
{
    TIdCollection<int> items;
    items.RestoreNextId(TId<int>{12});

    EXPECT_EQ(items.Size(), 0u);
    EXPECT_EQ(items.Add(10).Value, 12u);
}

TEST(IdCollectionTest, DuplicateRestoreDoesNotChangeEntryOrAllocator)
{
    TIdCollection<int> items;
    items.RestoreEntry(TId<int>{4}, 10);
    items.RestoreNextId(TId<int>{12});

    EXPECT_THROW(items.RestoreEntry(TId<int>{4}, 20), std::invalid_argument);

    EXPECT_EQ(items.Size(), 1u);
    EXPECT_EQ(*items.Find(TId<int>{4}), 10);
    EXPECT_EQ(items.NextId().Value, 12u);
}

TEST(IdCollectionTest, RejectsNextIdAtOrBelowLargestEntry)
{
    TIdCollection<int> items;
    items.RestoreEntry(TId<int>{4}, 10);
    items.RestoreNextId(TId<int>{12});

    EXPECT_THROW(items.RestoreNextId(TId<int>{4}), std::invalid_argument);
    EXPECT_THROW(items.RestoreNextId(TId<int>{0}), std::invalid_argument);

    EXPECT_EQ(items.NextId().Value, 12u);
    EXPECT_EQ(items.Add(20).Value, 12u);
}

TEST(IdCollectionTest, RejectsEntryWithoutRepresentableSuccessor)
{
    TIdCollection<int> items;
    const auto invalid = TId<int>{std::numeric_limits<std::size_t>::max()};

    EXPECT_THROW(items.RestoreEntry(invalid, 10), std::invalid_argument);

    EXPECT_EQ(items.Size(), 0u);
    EXPECT_EQ(items.NextId().Value, 0u);
}

TEST(IdCollectionTest, ExhaustedAllocatorDoesNotWrapOrReuseIds)
{
    TIdCollection<int> items;
    const auto last = TId<int>{std::numeric_limits<std::size_t>::max() - 1};
    items.RestoreEntry(last, 10);

    EXPECT_EQ(items.NextId().Value, std::numeric_limits<std::size_t>::max());
    EXPECT_THROW(items.Add(20), std::overflow_error);
    EXPECT_EQ(items.Size(), 1u);
    EXPECT_EQ(*items.Find(last), 10);

    TIdCollection<int> empty;
    empty.RestoreNextId(items.NextId());
    EXPECT_THROW(empty.Add(20), std::overflow_error);
    EXPECT_EQ(empty.Size(), 0u);
}

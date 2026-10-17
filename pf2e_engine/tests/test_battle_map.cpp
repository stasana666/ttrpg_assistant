#include <gtest/gtest.h>

#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/inventory/battle_maps.h>

TEST(BattleMap, NonSquareMapAddressesCornersInBounds) {
    // Wider than tall: exercises the (x, y) -> [y][x] indexing on a shape where
    // a transposed access would read out of bounds.
    TBattleMap map(6, 3);

    EXPECT_EQ(map.GetXSize(), 6);
    EXPECT_EQ(map.GetYSize(), 3);

    TPlayer* sentinel = reinterpret_cast<TPlayer*>(0x1);

    // Far corners: x at max width, y at max height.
    map.GetCell(5, 0).player = sentinel;
    map.GetCell(0, 2).player = sentinel;

    EXPECT_EQ(map.GetCell(5, 0).player, sentinel);
    EXPECT_EQ(map.GetCell(0, 2).player, sentinel);

    // Distinct (x, y) map to distinct cells (no transposition aliasing).
    map.GetCell(1, 0).player = nullptr;
    map.GetCell(0, 1).player = reinterpret_cast<TPlayer*>(0x2);
    EXPECT_EQ(map.GetCell(1, 0).player, nullptr);
    EXPECT_EQ(map.GetCell(0, 1).player, reinterpret_cast<TPlayer*>(0x2));
}

TEST(BattleMap, DeclaredSimpleMapIsTenByTen) {
    const auto layout = CreateSimpleMap();
    EXPECT_EQ(layout.Width(), 10);
    EXPECT_EQ(layout.Height(), 10);
}

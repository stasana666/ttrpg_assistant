#include <gtest/gtest.h>

#include <mock_dice_roller.h>

#include <pf2e_engine/creature.h>
#include <pf2e_engine/expressions/expressions.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/mechanics/hitpoints.h>
#include <pf2e_engine/random.h>

#include <memory>

TEST(ExpressionTest, OneDice) {
    TMockRng rng;
    rng.ExpectCall(20, 1);

    std::unique_ptr<IExpression> dice = std::make_unique<TDiceExpression>(20);

    EXPECT_EQ(dice->Value(rng), 1);
    rng.Verify();
}

TEST(ExpressionTest, DiceExpression) {
    TMockRng rng;
    rng.ExpectCall(20, 1);
    rng.ExpectCall(4, 1);

    std::unique_ptr<IExpression> skill_check = std::make_unique<TSumExpression>(
        std::make_unique<TDiceExpression>(20),
        std::make_unique<TSumExpression>(
            std::make_unique<TNumberExpression>(2),
            std::make_unique<TDiceExpression>(4)
        )
    );

    EXPECT_EQ(skill_check->Value(rng), 4);
    rng.Verify();
}

TEST(ExpressionTest, MultiplyExpression) {
    TRandomGenerator _(666);
    std::unique_ptr<IExpression> expr =
        std::make_unique<TProductExpression>(
            std::make_unique<TNumberExpression>(6),
            std::make_unique<TNumberExpression>(8)
        );
    EXPECT_EQ(expr->Value(_), 48);
}

TEST(ExpressionTest, MultiDiceExpression) {
    TMockRng rng;
    // 6d6: expect 6 calls to d6
    rng.ExpectCall(6, 1);
    rng.ExpectCall(6, 2);
    rng.ExpectCall(6, 3);
    rng.ExpectCall(6, 4);
    rng.ExpectCall(6, 5);
    rng.ExpectCall(6, 6);

    std::unique_ptr<IExpression> expr = std::make_unique<TMultiDiceExpression>(6, 6);
    EXPECT_EQ(expr->Value(rng), 21);  // 1+2+3+4+5+6 = 21
    rng.Verify();
}

TEST(ExpressionTest, MultiDiceExpressionSingle) {
    TMockRng rng;
    rng.ExpectCall(8, 5);

    std::unique_ptr<IExpression> expr = std::make_unique<TMultiDiceExpression>(1, 8);
    EXPECT_EQ(expr->Value(rng), 5);
    rng.Verify();
}

TEST(DiceExpressionParserTest, Parse6d6) {
    auto expr = ParseDiceExpression("6d6");

    TMockRng rng;
    for (int i = 0; i < 6; ++i) {
        rng.ExpectCall(6, 3);
    }

    EXPECT_EQ(expr->Value(rng), 18);  // 6 * 3 = 18
    rng.Verify();
}

TEST(DiceExpressionParserTest, Parse2d8) {
    auto expr = ParseDiceExpression("2d8");

    TMockRng rng;
    rng.ExpectCall(8, 4);
    rng.ExpectCall(8, 7);

    EXPECT_EQ(expr->Value(rng), 11);  // 4 + 7 = 11
    rng.Verify();
}

TEST(DiceExpressionParserTest, Parse1d20) {
    auto expr = ParseDiceExpression("1d20");

    TMockRng rng;
    rng.ExpectCall(20, 15);

    EXPECT_EQ(expr->Value(rng), 15);
    rng.Verify();
}

TEST(DiceExpressionParserTest, InvalidExpressionMissingD) {
    EXPECT_THROW(ParseDiceExpression("66"), std::invalid_argument);
}

TEST(DiceExpressionParserTest, InvalidExpressionEmptyCount) {
    EXPECT_THROW(ParseDiceExpression("d6"), std::invalid_argument);
}

TEST(DiceExpressionParserTest, InvalidExpressionEmptySize) {
    EXPECT_THROW(ParseDiceExpression("6d"), std::invalid_argument);
}

/*
TEST(ExpressionTest, CreatureExpression) {
    TGameContext ctx;

    TGameObjectIdManager game_object_manager;
    TGameObjectId target_id = game_object_manager.Register("warrior");

    TCharacteristicSet target_stats({10, 16, 10, 10, 10, 10});
    TArmor target_armor(3, 2);
    TCreature target(target_stats, target_armor, THitPoints{10});

    TGameObjectStorage game_object_storage;
    ctx.gameObjectStorage = &game_object_storage;
    game_object_storage.Add(target_id, &target);

    std::unique_ptr<IExpression> expr = std::make_unique<TArmorClassExpression>(target_id);

    EXPECT_EQ(expr->Value(ctx), 15);
}
*/

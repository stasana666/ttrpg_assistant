#include <gtest/gtest.h>

#include "dice.h"
#include <mock_dice_roller.h>

TEST(DiceTest, OneDice) {
    TMockRng rng;
    rng.ExpectCall(20, 1);
    std::unique_ptr<IExpression> dice = std::make_unique<TDice>(20);

    EXPECT_EQ(dice->Value(rng), 1);
    rng.Verify();
}

TEST(DiceTest, DiceExpression) {
    TMockRng rng;
    rng.ExpectCall(20, 1);
    rng.ExpectCall(4, 1);
    std::unique_ptr<IExpression> skillCheck = std::make_unique<TSumExpression>(
        std::make_unique<TDice>(20),
        std::make_unique<TSumExpression>(
            std::make_unique<TNumber>(2),
            std::make_unique<TDice>(4)
        )
    );

    EXPECT_EQ(skillCheck->Value(rng), 4);
    rng.Verify();
}

TEST(DiceTest, MultiplyExpression) {
    TMockRng _;
    std::unique_ptr<IExpression> expr = 
        std::make_unique<TMultiplyExpression>(
            std::make_unique<TNumber>(6),
            std::make_unique<TNumber>(8)
        );
    EXPECT_EQ(expr->Value(_), 48);
    _.Verify();
}

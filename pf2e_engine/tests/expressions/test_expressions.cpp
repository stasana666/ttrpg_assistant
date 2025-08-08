#include <gtest/gtest.h>

#include <mock_dice_roller.h>

#include <pf2e_engine/creature.h>
#include <pf2e_engine/expressions/expressions.h>
#include <pf2e_engine/game_context.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/mechanics/hitpoints.h>

#include <memory>

TEST(ExpressionTest, OneDice) {
    TMockRng rng;
    rng.ExpectCall(20, 1);
    TGameContext ctx{
        .gameObjectStorage = nullptr,
        .diceRoller = &rng
    };

    std::unique_ptr<IExpression> dice = std::make_unique<TDiceExpression>(20);

    EXPECT_EQ(dice->Value(ctx), 1);
    rng.Verify();
}

TEST(ExpressionTest, DiceExpression) {
    TMockRng rng;
    rng.ExpectCall(20, 1);
    rng.ExpectCall(4, 1);
    TGameContext ctx{
        .gameObjectStorage = nullptr,
        .diceRoller = &rng
    };

    std::unique_ptr<IExpression> skillCheck = std::make_unique<TSumExpression>(
        std::make_unique<TDiceExpression>(20),
        std::make_unique<TSumExpression>(
            std::make_unique<TNumberExpression>(2),
            std::make_unique<TDiceExpression>(4)
        )
    );

    EXPECT_EQ(skillCheck->Value(ctx), 4);
    rng.Verify();
}

TEST(ExpressionTest, MultiplyExpression) {
    TGameContext _;
    std::unique_ptr<IExpression> expr = 
        std::make_unique<TProductExpression>(
            std::make_unique<TNumberExpression>(6),
            std::make_unique<TNumberExpression>(8)
        );
    EXPECT_EQ(expr->Value(_), 48);
}

TEST(ExpressionTest, CreatureExpression) {
    TGameContext ctx;

    TGameObjectIdManager gameObjectManager;
    TGameObjectId targetId = gameObjectManager.Register("warrior");

    TCharacteristicSet targetStats({10, 16, 10, 10, 10, 10});
    TArmor targetArmor(3, 2);
    TCreature target(targetStats, targetArmor, THitPoints{10});

    TGameObjectStorage gameObjectStorage;
    ctx.gameObjectStorage = &gameObjectStorage;
    gameObjectStorage.Add(targetId, &target);

    std::unique_ptr<IExpression> expr = std::make_unique<TArmorClassExpression>(targetId);

    EXPECT_EQ(expr->Value(ctx), 15);
}

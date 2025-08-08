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

    std::unique_ptr<IExpression> skill_check = std::make_unique<TSumExpression>(
        std::make_unique<TDiceExpression>(20),
        std::make_unique<TSumExpression>(
            std::make_unique<TNumberExpression>(2),
            std::make_unique<TDiceExpression>(4)
        )
    );

    EXPECT_EQ(skill_check->Value(ctx), 4);
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

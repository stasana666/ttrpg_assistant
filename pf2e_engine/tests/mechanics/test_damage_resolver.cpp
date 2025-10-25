#include <gtest/gtest.h>

#include <pf2e_engine/expressions/dice_expression.h>
#include <pf2e_engine/expressions/number_expression.h>
#include <pf2e_engine/expressions/math_expression.h>

#include <damage_resolver.h>
#include <mock_dice_roller.h>

TEST(DamageResolverTest, JustDamage) {
    TMockRng rng;
    rng.ExpectCall(6, 5);

    TDamage damage;
    damage.Add(TDamage::Type::Slashing, std::make_unique<TDiceExpression>(6));

    TDamageResolver damage_resolver;
    EXPECT_EQ(damage_resolver(damage, rng), 5);
    rng.Verify();
}

TEST(DamageResolverTest, NegativeDamageToZero) {
    TMockRng rng;
    rng.ExpectCall(6, 1);

    TDamage damage;
    damage.Add(TDamage::Type::Slashing, std::make_unique<TSumExpression>(
        std::make_unique<TDiceExpression>(6),
        std::make_unique<TNumberExpression>(-5)
    ));

    TDamageResolver damage_resolver;
    EXPECT_EQ(damage_resolver(damage, rng), 1);
    rng.Verify();
}

TEST(DamageResolverTest, DoubleSliceOneType) {
    TDamage damage;
    damage.Add(TDamage::Type::Slashing, std::make_unique<TDiceExpression>(6));
    damage.Add(TDamage::Type::Slashing, std::make_unique<TDiceExpression>(6));

    TMockRng rng;
    rng.ExpectCall(6, 6);
    rng.ExpectCall(6, 4);

    TDamageResolver damage_resolver;
    EXPECT_EQ(damage_resolver(damage, rng), 10);
    rng.Verify();
}

TEST(DamageResolverTest, DoubleSliceTwoTypes) {
    TMockRng rng;
    rng.ExpectCall(6, 6);
    rng.ExpectCall(6, 4);

    TDamage damage;
    damage.Add(TDamage::Type::Slashing, std::make_unique<TDiceExpression>(6));
    damage.Add(TDamage::Type::Bludgeoning, std::make_unique<TDiceExpression>(6));

    TDamageResolver damage_resolver;
    EXPECT_EQ(damage_resolver(damage, rng), 10);
    rng.Verify();
}

TEST(DamageResolverTest, Immunity) {
    TMockRng rng;
    rng.ExpectCall(6, 6);
    rng.ExpectCall(6, 4);

    TDamage damage;
    damage.Add(TDamage::Type::Slashing, std::make_unique<TDiceExpression>(6));
    damage.Add(TDamage::Type::Bludgeoning, std::make_unique<TDiceExpression>(6));
    
    TDamageResolver damage_resolver;
    damage_resolver.AddImmunity(TDamage::Type::Slashing);

    int result = damage_resolver(damage, rng);
    EXPECT_TRUE(result == 4 || result == 6);
    rng.Verify();
}

TEST(DamageResolverTest, TwoTypesDiceRolling) {
    TDamage damage;
    damage.Add(TDamage::Type::Slashing, std::make_unique<TDiceExpression>(6));
    damage.Add(TDamage::Type::Bludgeoning, std::make_unique<TDiceExpression>(6));

    TDamageResolver damage_resolver;
    damage_resolver.AddImmunity(TDamage::Type::Slashing);

    std::vector<int> results;
    std::vector<int> lh;
    std::vector<int> rh;
    for (int i = 1; i <= 6; ++i) {
        for (int j = 1; j <= 6; ++j) {
            TMockRng rng;
            rng.ExpectCall(6, i);
            rng.ExpectCall(6, j);

            results.emplace_back(damage_resolver(damage, rng));
            lh.emplace_back(i);
            rh.emplace_back(j);
            rng.Verify();
        }
    }

    EXPECT_TRUE(results == lh || results == rh);
}

TEST(DamageResolverTest, Resistance) {
    TDamage damage;
    damage.Add(TDamage::Type::Slashing, std::make_unique<TDiceExpression>(6));

    TDamageResolver damage_resolver;
    damage_resolver.AddResistance(TDamage::Type::Slashing, 5);

    TMockRng rng;

    rng.ExpectCall(6, 4);
    EXPECT_TRUE(damage_resolver(damage, rng) == 0);
    rng.Verify();

    rng.ExpectCall(6, 5);
    EXPECT_TRUE(damage_resolver(damage, rng) == 0);
    rng.Verify();

    rng.ExpectCall(6, 6);
    EXPECT_TRUE(damage_resolver(damage, rng) == 1);
    rng.Verify();
}

TEST(DamageResolverTest, ResistanceDoubleSlice) {
    TDamage damage;
    damage.Add(TDamage::Type::Slashing, std::make_unique<TDiceExpression>(6));
    damage.Add(TDamage::Type::Slashing, std::make_unique<TDiceExpression>(6));

    TDamageResolver damage_resolver;
    damage_resolver.AddResistance(TDamage::Type::Slashing, 5);

    TMockRng rng;
    rng.ExpectCall(6, 6);
    rng.ExpectCall(6, 6);

    EXPECT_TRUE(damage_resolver(damage, rng) == 7);
    rng.Verify();
}

TEST(DamageResolverTest, ResistanceTwoTypes) {
    TDamage damage;
    damage.Add(TDamage::Type::Slashing, std::make_unique<TDiceExpression>(6));
    damage.Add(TDamage::Type::Bludgeoning, std::make_unique<TDiceExpression>(6));

    TDamageResolver damage_resolver;
    damage_resolver.AddResistance(TDamage::Type::Slashing, 5);

    TMockRng rng;
    rng.ExpectCall(6, 6);
    rng.ExpectCall(6, 6);

    EXPECT_TRUE(damage_resolver(damage, rng) == 7);
    rng.Verify();
}

TEST(DamageResolverTest, Vulnerability) {
    TDamage damage;
    damage.Add(TDamage::Type::Slashing, std::make_unique<TDiceExpression>(6));

    TDamageResolver damage_resolver;
    damage_resolver.AddVulnerability(TDamage::Type::Slashing, 5);

    TMockRng rng;

    rng.ExpectCall(6, 4);
    EXPECT_TRUE(damage_resolver(damage, rng) == 9);
    rng.Verify();
}

TEST(DamageResolverTest, VulnerabilityDoubleSlice) {
    TDamage damage;
    damage.Add(TDamage::Type::Slashing, std::make_unique<TDiceExpression>(6));
    damage.Add(TDamage::Type::Slashing, std::make_unique<TDiceExpression>(6));

    TDamageResolver damage_resolver;
    damage_resolver.AddVulnerability(TDamage::Type::Slashing, 5);

    TMockRng rng;
    rng.ExpectCall(6, 5);
    rng.ExpectCall(6, 5);

    EXPECT_TRUE(damage_resolver(damage, rng) == 15);
    rng.Verify();
}

TEST(DamageResolverTest, VulnerabilityTwoTypes) {
    TDamage damage;
    damage.Add(TDamage::Type::Slashing, std::make_unique<TDiceExpression>(6));
    damage.Add(TDamage::Type::Bludgeoning, std::make_unique<TDiceExpression>(6));

    TDamageResolver damage_resolver;
    damage_resolver.AddVulnerability(TDamage::Type::Slashing, 5);

    TMockRng rng;
    rng.ExpectCall(6, 5);
    rng.ExpectCall(6, 5);

    EXPECT_TRUE(damage_resolver(damage, rng) == 15);
    rng.Verify();
}

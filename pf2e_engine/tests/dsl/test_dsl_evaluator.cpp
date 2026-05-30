#include <gtest/gtest.h>

#include <pf2e_engine/dsl/builtins.h>
#include <pf2e_engine/dsl/expression.h>
#include <pf2e_engine/dsl/parser.h>
#include <pf2e_engine/dsl/value_convert.h>

#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/game_object_logic/game_object.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/mechanics/damage.h>

#include <memory>
#include <string>

namespace {

class DslEvaluatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        EnsureDslBuiltinsRegistered();
        ctx_.registry = std::make_shared<TGameObjectRegistry>();
    }

    void Bind(const std::string& name, TGameObjectPtr value) {
        ctx_.registry->Add(TGameObjectIdManager::Instance().Register(name), value);
    }

    TDslValue Eval(const std::string& src) {
        auto expr = ParseDsl(src);
        return expr->Evaluate(ctx_);
    }

    TEvalContext ctx_;
};

}  // namespace

TEST_F(DslEvaluatorTest, RegistryVariableLookup) {
    Bind("x", 42);
    EXPECT_EQ(Eval("$x").AsInt(), 42);
}

TEST_F(DslEvaluatorTest, MissingVariableThrows) {
    EXPECT_THROW(Eval("$missing"), std::runtime_error);
}

TEST_F(DslEvaluatorTest, WeaponReachProperty) {
    TWeapon w("longsword", 8, TDamage::Type::Slashing, EWeaponCategory::Martial, 1);
    Bind("weapon", &w);
    EXPECT_EQ(Eval("$weapon.reach").AsInt(), 1);

    TWeapon glaive("glaive", 8, TDamage::Type::Slashing, EWeaponCategory::Martial, 2);
    Bind("polearm", &glaive);
    EXPECT_EQ(Eval("$polearm.reach").AsInt(), 2);
    EXPECT_TRUE(Eval("$polearm.reach > $weapon.reach").AsBool());
}

TEST_F(DslEvaluatorTest, ScopeShadowsRegistry) {
    Bind("x", 1);
    EXPECT_EQ(Eval("$x").AsInt(), 1);
    {
        TScopeGuard guard(ctx_, "x", TDslValue(99));
        EXPECT_EQ(Eval("$x").AsInt(), 99);
    }
    EXPECT_EQ(Eval("$x").AsInt(), 1);
}

TEST_F(DslEvaluatorTest, NestedScopeRestoresPriorBinding) {
    Bind("x", 1);
    {
        TScopeGuard outer(ctx_, "x", TDslValue(10));
        {
            TScopeGuard inner(ctx_, "x", TDslValue(20));
            EXPECT_EQ(Eval("$x").AsInt(), 20);
        }
        EXPECT_EQ(Eval("$x").AsInt(), 10);
    }
    EXPECT_EQ(Eval("$x").AsInt(), 1);
}

TEST_F(DslEvaluatorTest, UnknownPropertyThrows) {
    TWeapon w("dagger", 4, TDamage::Type::Piercing, EWeaponCategory::Simple, 1);
    Bind("weapon", &w);
    EXPECT_THROW(Eval("$weapon.no_such_property"), std::runtime_error);
}

TEST_F(DslEvaluatorTest, PropertyOnNonObjectThrows) {
    Bind("x", 42);
    EXPECT_THROW(Eval("$x.reach"), std::runtime_error);
}

TEST_F(DslEvaluatorTest, BoolPredicateOnIntComparison) {
    Bind("a", 5);
    Bind("b", 3);
    EXPECT_TRUE(Eval("$a > $b").AsBool());
    EXPECT_TRUE(Eval("$a >= $a").AsBool());
    EXPECT_FALSE(Eval("$a == $b").AsBool());
}

TEST_F(DslEvaluatorTest, MinFunctionWithVariables) {
    Bind("a", 10);
    Bind("b", 3);
    EXPECT_EQ(Eval("min($a, $b)").AsInt(), 3);
    EXPECT_EQ(Eval("max($a, $b)").AsInt(), 10);
}

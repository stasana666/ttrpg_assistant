#include <gtest/gtest.h>

#include <pf2e_engine/dsl/builtins.h>
#include <pf2e_engine/dsl/expression.h>
#include <pf2e_engine/dsl/parser.h>

#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/material.h>

#include <cpp_config.h>

#include <nlohmann/json.hpp>

#include <memory>
#include <string>

namespace {

class DslArmorPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        EnsureDslBuiltinsRegistered();
        ctx_.scope.clear();

        // The test factory holds a single "steel" material so generated
        // FromJson can resolve the ref TMaterial Material field.
        factory_.AddSource(kRootDirPath + "/pf2e_engine/data/inventory/material/steel.json");
    }

    TDslValue Eval(const std::string& src) {
        auto expr = ParseDsl(src);
        return expr->Evaluate(ctx_);
    }

    // Build a fully-populated armor JSON suitable for the generated FromJson.
    static nlohmann::json MakeArmorJson(const std::string& category, int ac, int dex) {
        return {
            {"category", category},
            {"armor_class_bonus", ac},
            {"dexterity_cap", dex},
            {"material", "steel"},
        };
    }

    TEvalContext ctx_;
    TGameObjectFactory factory_;
};

}  // namespace

TEST_F(DslArmorPropertyTest, GeneratedAcBonusProperty) {
    TArmor armor = TArmor::FromJson(MakeArmorJson("Medium", 4, 2), factory_);

    ctx_.scope.emplace("armor", TDslValue(&armor));

    EXPECT_EQ(Eval("$armor.armor_class_bonus").AsInt(), 4);
    EXPECT_EQ(Eval("$armor.dexterity_cap").AsInt(), 2);
    EXPECT_TRUE(Eval("$armor.armor_class_bonus >= $armor.dexterity_cap").AsBool());
}

TEST_F(DslArmorPropertyTest, FromJsonRoundTrip) {
    TArmor armor = TArmor::FromJson(MakeArmorJson("Heavy", 6, 0), factory_);

    EXPECT_EQ(armor.Category(), EArmorCategory::Heavy);
    EXPECT_EQ(armor.ArmorClassBonus(), 6);
    EXPECT_EQ(armor.DexterityCap(), 0);
}

TEST_F(DslArmorPropertyTest, DefaultsApplied) {
    TArmor armor;
    EXPECT_EQ(armor.Category(), EArmorCategory::Unarmored);
    EXPECT_EQ(armor.ArmorClassBonus(), 0);
    EXPECT_EQ(armor.DexterityCap(), std::numeric_limits<int>::max());
}

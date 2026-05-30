#include <gtest/gtest.h>

#include <pf2e_engine/dsl/builtins.h>
#include <pf2e_engine/dsl/expression.h>
#include <pf2e_engine/dsl/parser.h>

#include <pf2e_engine/inventory/armor.h>

#include <nlohmann/json.hpp>

#include <memory>
#include <string>

namespace {

class DslArmorPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        EnsureDslBuiltinsRegistered();
        ctx_.scope.clear();
    }

    TDslValue Eval(const std::string& src) {
        auto expr = ParseDsl(src);
        return expr->Evaluate(ctx_);
    }

    TEvalContext ctx_;
};

}  // namespace

TEST_F(DslArmorPropertyTest, GeneratedAcBonusProperty) {
    nlohmann::json j = {
        {"category", "Medium"},
        {"armor_class_bonus", 4},
        {"dexterity_cap", 2},
    };
    TArmor armor = TArmor::FromJson(j);

    ctx_.scope.emplace("armor", TDslValue(&armor));

    EXPECT_EQ(Eval("$armor.armor_class_bonus").AsInt(), 4);
    EXPECT_EQ(Eval("$armor.dexterity_cap").AsInt(), 2);
    EXPECT_TRUE(Eval("$armor.armor_class_bonus >= $armor.dexterity_cap").AsBool());
}

TEST_F(DslArmorPropertyTest, FromJsonRoundTrip) {
    nlohmann::json j = {
        {"category", "Heavy"},
        {"armor_class_bonus", 6},
        {"dexterity_cap", 0},
    };
    TArmor armor = TArmor::FromJson(j);

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

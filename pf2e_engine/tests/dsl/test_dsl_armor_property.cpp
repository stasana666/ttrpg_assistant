#include <gtest/gtest.h>

#include <pf2e_engine/dsl/builtins.h>
#include <pf2e_engine/dsl/expression.h>
#include <pf2e_engine/dsl/parser.h>

#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/material.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

#include "mock_interaction_system.h"

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

        factory_.AddSource(kRootDirPath + "/pf2e_engine/data/inventory/material/steel.json");
    }

    TDslValue Eval(const std::string& src) {
        auto expr = ParseDsl(src);
        return expr->Evaluate(ctx_);
    }

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

}

TEST_F(DslArmorPropertyTest, GeneratedAcBonusProperty) {
    TArmor armor = TArmor::FromJson(MakeArmorJson("Medium", 4, 2), factory_);

    ctx_.scope.emplace("armor", TDslValue(&armor));

    EXPECT_EQ(Eval("$armor.armor_class_bonus").AsInt(), 4);
    EXPECT_EQ(Eval("$armor.dexterity_cap").AsInt(), 2);
    EXPECT_TRUE(Eval("$armor.armor_class_bonus >= $armor.dexterity_cap").AsBool());
}

TEST_F(DslArmorPropertyTest, FromJsonRoundTrip) {
    const TArmor armor = TArmor::FromJson(MakeArmorJson("Heavy", 6, 0), factory_);

    EXPECT_EQ(armor.Category(), EArmorCategory::Heavy);
    EXPECT_EQ(armor.ArmorClassBonus(), 6);
    EXPECT_EQ(armor.DexterityCap(), 0);
}

TEST_F(DslArmorPropertyTest, DefaultsApplied) {
    const TArmor armor;
    EXPECT_EQ(armor.Category(), EArmorCategory::Unarmored);
    EXPECT_EQ(armor.ArmorClassBonus(), 0);
    EXPECT_EQ(armor.DexterityCap(), std::numeric_limits<int>::max());
}

TEST_F(DslArmorPropertyTest, GeneratedCreaturePropertiesFollowStateAndRollback) {
    auto creature = TCreature::Create({.Level = 1, .Movement = 5});
    TPlayer player(&creature, TPlayerTeam{1}, TPlayerId{1}, "Fighter", "");
    ctx_.scope.emplace("actor", TDslValue(&player));
    TMockInteractionSystem io;
    TTransformator transformator(io);
    TGeneratedExecutionContext execution(transformator);
    const auto before = transformator.CurrentState();

    EXPECT_EQ(Eval("$actor.creature.level").AsInt(), 1);
    EXPECT_EQ(Eval("$actor.creature.movement").AsInt(), 5);
    execution.AssignGuarded(creature.Level(), 4, TGuarded<TCreature>(creature), "level");
    EXPECT_EQ(Eval("$actor.creature.level").AsInt(), 4);
    transformator.Undo(before);
    EXPECT_EQ(Eval("$actor.creature.level").AsInt(), 1);
}

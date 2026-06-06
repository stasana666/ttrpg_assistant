#include <gtest/gtest.h>

#include <pf2e_engine/dsl/parser.h>
#include <pf2e_engine/dsl/builtins.h>
#include <pf2e_engine/dsl/expression.h>
#include <pf2e_engine/dsl/function_registry.h>

#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

#include <memory>

namespace {

TDslValue Eval(const std::string& src) {
    EnsureDslBuiltinsRegistered();
    TEvalContext ctx;
    ctx.registry = std::make_shared<TGameObjectRegistry>();
    auto expr = ParseDsl(src);
    return expr->Evaluate(ctx);
}

}


TEST(DslParserTest, IntegerLiteral) {
    TDslValue v = Eval("42");
    ASSERT_TRUE(v.Is<int>());
    EXPECT_EQ(v.AsInt(), 42);
}

TEST(DslParserTest, ComparisonReturnsBool) {
    EXPECT_TRUE(Eval("3 < 5").AsBool());
    EXPECT_FALSE(Eval("5 < 3").AsBool());
    EXPECT_TRUE(Eval("5 == 5").AsBool());
    EXPECT_FALSE(Eval("5 == 4").AsBool());
    EXPECT_TRUE(Eval("5 != 4").AsBool());
    EXPECT_TRUE(Eval("5 >= 5").AsBool());
    EXPECT_TRUE(Eval("4 <= 5").AsBool());
}

TEST(DslParserTest, LogicalShortCircuit) {
    EXPECT_FALSE(Eval("(1 == 2) && $undefined").AsBool());
    EXPECT_TRUE(Eval("(1 == 1) || $undefined").AsBool());
}

TEST(DslParserTest, NotOperator) {
    EXPECT_FALSE(Eval("!(3 < 5)").AsBool());
    EXPECT_TRUE(Eval("!(3 > 5)").AsBool());
}

TEST(DslParserTest, Parentheses) {
    EXPECT_TRUE(Eval("(1 == 2) || (1 == 1) && (1 == 1)").AsBool());
    EXPECT_FALSE(Eval("((1 == 2) || (1 == 1)) && (1 == 2)").AsBool());
}

TEST(DslParserTest, Arithmetic) {
    EXPECT_EQ(Eval("2 + 3 * 4").AsInt(), 14);
    EXPECT_EQ(Eval("(2 + 3) * 4").AsInt(), 20);
    EXPECT_EQ(Eval("10 - 4 - 3").AsInt(), 3);
    EXPECT_TRUE(Eval("2 + 2 == 4").AsBool());
}

TEST(DslParserTest, MinMaxBuiltins) {
    EXPECT_EQ(Eval("min(3, 7)").AsInt(), 3);
    EXPECT_EQ(Eval("max(3, 7)").AsInt(), 7);
    EXPECT_EQ(Eval("min(max(2, 5), 4)").AsInt(), 4);
}

TEST(DslParserTest, UnknownFunctionThrows) {
    EXPECT_THROW(Eval("nonexistent_fn(1, 2)"), std::runtime_error);
}

TEST(DslParserTest, UnterminatedCallThrows) {
    EXPECT_THROW(ParseDsl("min(1, 2"), std::runtime_error);
}

TEST(DslParserTest, BadTokenAfterDotThrows) {
    EXPECT_THROW(ParseDsl("$x.5"), std::runtime_error);
}

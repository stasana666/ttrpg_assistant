#include <gtest/gtest.h>

#include <pf2e_engine/dsl/lexer.h>
#include <pf2e_engine/dsl/parser.h>
#include <pf2e_engine/dsl/builtins.h>
#include <pf2e_engine/dsl/expression.h>
#include <pf2e_engine/dsl/function_registry.h>

#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

#include <memory>

namespace {

// Evaluate a DSL expression with no scope/registry context — only literals
// and registered builtin functions are accessible.
TDslValue Eval(const std::string& src) {
    EnsureDslBuiltinsRegistered();
    TEvalContext ctx;
    ctx.registry = std::make_shared<TGameObjectRegistry>();
    auto expr = ParseDsl(src);
    return expr->Evaluate(ctx);
}

}  // namespace

TEST(DslLexerTest, BasicTokens) {
    auto tokens = Tokenize("$item.reach >= 5");
    ASSERT_EQ(tokens.size(), 7u);  // $ item . reach >= 5 End
    EXPECT_EQ(tokens[0].type, ETokenType::Dollar);
    EXPECT_EQ(tokens[1].type, ETokenType::Identifier);
    EXPECT_EQ(tokens[1].text, "item");
    EXPECT_EQ(tokens[2].type, ETokenType::Dot);
    EXPECT_EQ(tokens[3].type, ETokenType::Identifier);
    EXPECT_EQ(tokens[3].text, "reach");
    EXPECT_EQ(tokens[4].type, ETokenType::Ge);
    EXPECT_EQ(tokens[5].type, ETokenType::Number);
    EXPECT_EQ(tokens[5].number, 5);
}

TEST(DslLexerTest, LogicalAndComparison) {
    auto tokens = Tokenize("!a && (b || c) == d");
    // ! a && ( b || c ) == d End
    ASSERT_GE(tokens.size(), 10u);
    EXPECT_EQ(tokens[0].type, ETokenType::Not);
    EXPECT_EQ(tokens[2].type, ETokenType::And);
    EXPECT_EQ(tokens[5].type, ETokenType::Or);
    EXPECT_EQ(tokens[8].type, ETokenType::Eq);
}

TEST(DslLexerTest, RejectsUnknownChar) {
    EXPECT_THROW(Tokenize("@"), std::runtime_error);
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
    // && short-circuits: rhs is not evaluated when lhs is false. Use a
    // bogus rhs that would error if reached.
    EXPECT_FALSE(Eval("(1 == 2) && $undefined").AsBool());
    EXPECT_TRUE(Eval("(1 == 1) || $undefined").AsBool());
}

TEST(DslParserTest, NotOperator) {
    EXPECT_FALSE(Eval("!(3 < 5)").AsBool());
    EXPECT_TRUE(Eval("!(3 > 5)").AsBool());
}

TEST(DslParserTest, Parentheses) {
    // Without parens: && binds tighter than ||
    EXPECT_TRUE(Eval("(1 == 2) || (1 == 1) && (1 == 1)").AsBool());
    // With parens to force the other grouping:
    EXPECT_FALSE(Eval("((1 == 2) || (1 == 1)) && (1 == 2)").AsBool());
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

#include <gtest/gtest.h>

#include <expr/lexer.h>
#include <expr/parser.h>

#include <stdexcept>
#include <string>

using namespace expr;

TEST(ExprLexer, TokenizesOperatorsAndVars) {
    std::vector<TToken> toks = Tokenize("$a.b + f(1, -2) >= 3 && !x");
    // Spot-check a few kinds in order.
    EXPECT_EQ(toks.at(0).kind, ETok::Dollar);
    EXPECT_EQ(toks.at(1).kind, ETok::Ident);
    EXPECT_EQ(toks.at(2).kind, ETok::Dot);
    EXPECT_EQ(toks.back().kind, ETok::End);
}

TEST(ExprParser, ArithmeticPrecedence) {
    // '*' binds tighter than '+': a + (b * c).
    TExprNode e = Parse("a + b * c");
    ASSERT_EQ(e.Kind, ENodeKind::Binary);
    EXPECT_EQ(e.BinOp, EBinaryOp::Add);
    EXPECT_EQ(e.Lhs->Kind, ENodeKind::Var);
    EXPECT_EQ(e.Lhs->Text, "a");
    ASSERT_EQ(e.Rhs->Kind, ENodeKind::Binary);
    EXPECT_EQ(e.Rhs->BinOp, EBinaryOp::Mul);
}

TEST(ExprParser, MemberChain) {
    // a.b.c -> Member(Member(Var a, "b"), "c")
    TExprNode e = Parse("a.b.c");
    ASSERT_EQ(e.Kind, ENodeKind::Member);
    EXPECT_EQ(e.Text, "c");
    ASSERT_EQ(e.Lhs->Kind, ENodeKind::Member);
    EXPECT_EQ(e.Lhs->Text, "b");
    EXPECT_EQ(e.Lhs->Lhs->Kind, ENodeKind::Var);
    EXPECT_EQ(e.Lhs->Lhs->Text, "a");
}

TEST(ExprParser, MemberAccessInArithmetic) {
    // Race.Hitpoints + Level * (Class.Hitpoints + Characteristic.Constitution)
    TExprNode e = Parse(
        "Race.Hitpoints + Level * (Class.Hitpoints + Characteristic.Constitution)");
    ASSERT_EQ(e.Kind, ENodeKind::Binary);
    EXPECT_EQ(e.BinOp, EBinaryOp::Add);
    EXPECT_EQ(e.Lhs->Kind, ENodeKind::Member);   // Race.Hitpoints
    EXPECT_EQ(e.Rhs->Kind, ENodeKind::Binary);   // Level * (...)
    EXPECT_EQ(e.Rhs->BinOp, EBinaryOp::Mul);
}

TEST(ExprParser, DollarVarAndCall) {
    TExprNode e = Parse("min($a.reach, distance($a, $b))");
    ASSERT_EQ(e.Kind, ENodeKind::Call);
    EXPECT_EQ(e.Text, "min");
    ASSERT_EQ(e.Args.size(), 2u);
    EXPECT_EQ(e.Args.at(0).Kind, ENodeKind::Member);   // $a.reach
    EXPECT_EQ(e.Args.at(0).Lhs->Kind, ENodeKind::Var);
    EXPECT_TRUE(e.Args.at(0).Lhs->HasDollar);
    EXPECT_EQ(e.Args.at(1).Kind, ENodeKind::Call);     // distance(...)
}

TEST(ExprParser, ComparisonAndLogical) {
    TExprNode e = Parse("$x >= 1 && !$y");
    ASSERT_EQ(e.Kind, ENodeKind::Binary);
    EXPECT_EQ(e.BinOp, EBinaryOp::And);
    EXPECT_EQ(e.Lhs->Kind, ENodeKind::Binary);
    EXPECT_EQ(e.Lhs->BinOp, EBinaryOp::Ge);
    EXPECT_EQ(e.Rhs->Kind, ENodeKind::Unary);
    EXPECT_EQ(e.Rhs->UnOp, EUnaryOp::Not);
}

TEST(ExprParser, NegativeLiteralIsOneToken) {
    TExprNode e = Parse("-2");
    ASSERT_EQ(e.Kind, ENodeKind::IntLiteral);
    EXPECT_EQ(e.Text, "-2");
}

TEST(ExprParser, SubtractionIsBinary) {
    TExprNode e = Parse("a - 2");
    ASSERT_EQ(e.Kind, ENodeKind::Binary);
    EXPECT_EQ(e.BinOp, EBinaryOp::Sub);
}

TEST(ExprParser, ThrowsOnTrailingGarbage) {
    EXPECT_THROW(Parse("a +"), std::runtime_error);
    EXPECT_THROW(Parse("min(1, 2"), std::runtime_error);
}

#include <gtest/gtest.h>

#include <expr/lexer.h>
#include <expr/parser.h>

#include <stdexcept>
#include <string>

using namespace expr;

TEST(ExprLexer, TokenizesOperatorsAndVars) {
    std::vector<TToken> toks = Tokenize("$a.b + f(1, -2) >= 3 && !x");
    EXPECT_EQ(toks.at(0).kind, ETok::Dollar);
    EXPECT_EQ(toks.at(1).kind, ETok::Ident);
    EXPECT_EQ(toks.at(2).kind, ETok::Dot);
    EXPECT_EQ(toks.back().kind, ETok::End);
}

TEST(ExprParser, ArithmeticPrecedence) {
    TExprNode e = Parse("a + b * c");
    ASSERT_EQ(e.Kind, ENodeKind::Binary);
    EXPECT_EQ(e.BinOp, EBinaryOp::Add);
    EXPECT_EQ(e.Lhs->Kind, ENodeKind::Var);
    EXPECT_EQ(e.Lhs->Text, "a");
    ASSERT_EQ(e.Rhs->Kind, ENodeKind::Binary);
    EXPECT_EQ(e.Rhs->BinOp, EBinaryOp::Mul);
}

TEST(ExprParser, MemberChain) {
    TExprNode e = Parse("a.b.c");
    ASSERT_EQ(e.Kind, ENodeKind::Member);
    EXPECT_EQ(e.Text, "c");
    ASSERT_EQ(e.Lhs->Kind, ENodeKind::Member);
    EXPECT_EQ(e.Lhs->Text, "b");
    EXPECT_EQ(e.Lhs->Lhs->Kind, ENodeKind::Var);
    EXPECT_EQ(e.Lhs->Lhs->Text, "a");
}

TEST(ExprParser, MemberAccessInArithmetic) {
    TExprNode e = Parse(
        "Race.Hitpoints + Level * (Class.Hitpoints + Characteristic.Constitution)");
    ASSERT_EQ(e.Kind, ENodeKind::Binary);
    EXPECT_EQ(e.BinOp, EBinaryOp::Add);
    EXPECT_EQ(e.Lhs->Kind, ENodeKind::Member);
    EXPECT_EQ(e.Rhs->Kind, ENodeKind::Binary);
    EXPECT_EQ(e.Rhs->BinOp, EBinaryOp::Mul);
}

TEST(ExprParser, DollarVarAndCall) {
    TExprNode e = Parse("min($a.reach, distance($a, $b))");
    ASSERT_EQ(e.Kind, ENodeKind::Call);
    EXPECT_EQ(e.Text, "min");
    ASSERT_EQ(e.Args.size(), 2u);
    EXPECT_EQ(e.Args.at(0).Kind, ENodeKind::Member);
    EXPECT_EQ(e.Args.at(0).Lhs->Kind, ENodeKind::Var);
    EXPECT_TRUE(e.Args.at(0).Lhs->HasDollar);
    EXPECT_EQ(e.Args.at(1).Kind, ENodeKind::Call);
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

TEST(ExprParser, DistinguishesContextDollarAndLocalVariables) {
    const TExprNode context = Parse("@self");
    EXPECT_EQ(context.Kind, ENodeKind::ContextVar);
    EXPECT_EQ(context.Text, "self");
    EXPECT_FALSE(context.HasDollar);

    const TExprNode dollar = Parse("$self");
    EXPECT_EQ(dollar.Kind, ENodeKind::Var);
    EXPECT_TRUE(dollar.HasDollar);

    const TExprNode local = Parse("self");
    EXPECT_EQ(local.Kind, ENodeKind::Var);
    EXPECT_FALSE(local.HasDollar);
}

TEST(ExprParser, CallsMethodOnNestedContextObject) {
    const TExprNode e = Parse("@self.Hitpoints.Heal(Amount)");
    ASSERT_EQ(e.Kind, ENodeKind::Call);
    EXPECT_TRUE(e.Text.empty());
    ASSERT_NE(e.Lhs, nullptr);
    ASSERT_EQ(e.Lhs->Kind, ENodeKind::Member);
    EXPECT_EQ(e.Lhs->Text, "Heal");
    ASSERT_EQ(e.Lhs->Lhs->Kind, ENodeKind::Member);
    EXPECT_EQ(e.Lhs->Lhs->Text, "Hitpoints");
    EXPECT_EQ(e.Lhs->Lhs->Lhs->Kind, ENodeKind::ContextVar);
    ASSERT_EQ(e.Args.size(), 1u);
    EXPECT_EQ(e.Args[0].Text, "Amount");
}

TEST(ExprParser, PreservesNamedCallRepresentation) {
    const TExprNode e = Parse("max(0, min(Amount, @self.Maximum.Value))");
    ASSERT_EQ(e.Kind, ENodeKind::Call);
    EXPECT_EQ(e.Text, "max");
    EXPECT_EQ(e.Lhs, nullptr);
    ASSERT_EQ(e.Args.size(), 2u);
    EXPECT_EQ(e.Args[1].Text, "min");
    EXPECT_EQ(e.Args[1].Lhs, nullptr);
}

TEST(ExprParser, ChainsCallsMembersAndIndexes) {
    const TExprNode e = Parse("make().Attacks[Index + 1].Roll().Value");
    ASSERT_EQ(e.Kind, ENodeKind::Member);
    EXPECT_EQ(e.Text, "Value");
    const auto& roll = *e.Lhs;
    ASSERT_EQ(roll.Kind, ENodeKind::Call);
    EXPECT_TRUE(roll.Args.empty());
    ASSERT_EQ(roll.Lhs->Kind, ENodeKind::Member);
    EXPECT_EQ(roll.Lhs->Text, "Roll");
    const auto& index = *roll.Lhs->Lhs;
    ASSERT_EQ(index.Kind, ENodeKind::Index);
    EXPECT_EQ(index.Rhs->BinOp, EBinaryOp::Add);
    EXPECT_EQ(index.Lhs->Text, "Attacks");
    EXPECT_EQ(index.Lhs->Lhs->Text, "make");
}

TEST(ExprParser, CallsReturnedCallable) {
    const TExprNode e = Parse("factory()(Amount)");
    ASSERT_EQ(e.Kind, ENodeKind::Call);
    ASSERT_NE(e.Lhs, nullptr);
    EXPECT_EQ(e.Lhs->Kind, ENodeKind::Call);
    EXPECT_EQ(e.Lhs->Text, "factory");
    ASSERT_EQ(e.Args.size(), 1u);
    EXPECT_EQ(e.Args[0].Text, "Amount");
}

TEST(ExprParser, ParsesObjectInitializer) {
    const TExprNode e = Parse(
        "TDamageResult { Incoming = Amount; Before = @self.Current; After = 0; }");
    ASSERT_EQ(e.Kind, ENodeKind::ObjectInit);
    EXPECT_EQ(e.Text, "TDamageResult");
    ASSERT_EQ(e.Fields.size(), 3u);
    EXPECT_EQ(e.Fields[0].Name, "Incoming");
    EXPECT_EQ(e.Fields[0].Value.Text, "Amount");
    EXPECT_EQ(e.Fields[1].Name, "Before");
    EXPECT_EQ(e.Fields[1].Value.Kind, ENodeKind::Member);
    EXPECT_EQ(e.Fields[2].Value.Text, "0");
}

TEST(ExprParser, ParsesNestedAndEmptyInitializers) {
    const TExprNode e = Parse(
        "Creature { Name = \"Wolf\"; Hitpoints = THitPoints { Maximum = 24; }; Extra = Empty {}; }");
    ASSERT_EQ(e.Fields.size(), 3u);
    EXPECT_EQ(e.Fields[0].Value.Kind, ENodeKind::StringLiteral);
    EXPECT_EQ(e.Fields[0].Value.Text, "Wolf");
    const auto& hp = e.Fields[1].Value;
    ASSERT_EQ(hp.Kind, ENodeKind::ObjectInit);
    EXPECT_EQ(hp.Text, "THitPoints");
    ASSERT_EQ(hp.Fields.size(), 1u);
    EXPECT_EQ(hp.Fields[0].Name, "Maximum");
    EXPECT_EQ(hp.Fields[0].Value.Text, "24");
    EXPECT_TRUE(e.Fields[2].Value.Fields.empty());
}

TEST(ExprParser, UsesObjectInitializerInCallAndMemberAccess) {
    const TExprNode e = Parse("apply(TDamageResult { Incoming = Amount; }).After");
    ASSERT_EQ(e.Kind, ENodeKind::Member);
    EXPECT_EQ(e.Text, "After");
    ASSERT_EQ(e.Lhs->Kind, ENodeKind::Call);
    ASSERT_EQ(e.Lhs->Args.size(), 1u);
    EXPECT_EQ(e.Lhs->Args[0].Kind, ENodeKind::ObjectInit);
}

TEST(ExprParser, NegatesVariablesAndGroupedExpressions) {
    const TExprNode e = Parse("-@self.Current * -(Amount + 1)");
    ASSERT_EQ(e.Kind, ENodeKind::Binary);
    EXPECT_EQ(e.BinOp, EBinaryOp::Mul);
    ASSERT_EQ(e.Lhs->Kind, ENodeKind::Unary);
    EXPECT_EQ(e.Lhs->UnOp, EUnaryOp::Negate);
    EXPECT_EQ(e.Lhs->Lhs->Kind, ENodeKind::Member);
    ASSERT_EQ(e.Rhs->Kind, ENodeKind::Unary);
    EXPECT_EQ(e.Rhs->UnOp, EUnaryOp::Negate);
    EXPECT_EQ(e.Rhs->Lhs->BinOp, EBinaryOp::Add);
}

TEST(ExprParser, DistinguishesSubtractionAndSignedLiteralsWithoutWhitespace) {
    const TExprNode subtraction = Parse("Current-2");
    ASSERT_EQ(subtraction.Kind, ENodeKind::Binary);
    EXPECT_EQ(subtraction.BinOp, EBinaryOp::Sub);
    EXPECT_EQ(subtraction.Rhs->Text, "2");

    const TExprNode negative = Parse("Current--2");
    ASSERT_EQ(negative.Kind, ENodeKind::Binary);
    EXPECT_EQ(negative.BinOp, EBinaryOp::Sub);
    EXPECT_EQ(negative.Rhs->Kind, ENodeKind::IntLiteral);
    EXPECT_EQ(negative.Rhs->Text, "-2");

    const TExprNode indexed = Parse("Items[-2]-3");
    ASSERT_EQ(indexed.Kind, ENodeKind::Binary);
    EXPECT_EQ(indexed.Lhs->Rhs->Text, "-2");
    EXPECT_EQ(indexed.Rhs->Text, "3");
}

TEST(ExprParser, PreservesSourceLocationsInExpressionsAndFields) {
    const TExprNode e = Parse("Result {\n    Before = @self.Current;\n}");
    EXPECT_EQ(e.Location.line, 1);
    EXPECT_EQ(e.Location.col, 1);
    ASSERT_EQ(e.Fields.size(), 1u);
    EXPECT_EQ(e.Fields[0].Location.line, 2);
    EXPECT_EQ(e.Fields[0].Location.col, 5);
    const auto& self = *e.Fields[0].Value.Lhs;
    EXPECT_EQ(self.Location.line, 2);
    EXPECT_EQ(self.Location.col, 14);
}

TEST(ExprParser, DecodesEscapedStringLiterals) {
    const TExprNode e = Parse(R"("Wolf \"Grey\"\\\n\r\t")");
    EXPECT_EQ(e.Kind, ENodeKind::StringLiteral);
    EXPECT_EQ(e.Text, "Wolf \"Grey\"\\\n\r\t");
    EXPECT_EQ(Parse("\"\"").Text, "");
}

TEST(ExprParser, RejectsMalformedPostfixExpressionsAndInitializers) {
    for (const std::string source : {
             "@", "@$self", "@self.", "@self.Heal(,)", "@self.Heal(1,)",
             "@self.Items[]", "@self.Items[1", "Result { Incoming = 1 }",
             "Result { Incoming 1; }", "Result { Incoming = ; }", "Result {"}) {
        EXPECT_THROW(Parse(source), std::runtime_error) << source;
    }
}

TEST(ExprParser, RejectsInvalidStringsWithLocations) {
    EXPECT_THROW(Parse("\"unterminated"), std::runtime_error);
    EXPECT_THROW(Parse("\"line\nbreak\""), std::runtime_error);
    try {
        Parse(R"("bad\q")");
        FAIL() << "Expected invalid escape diagnostic";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("line 1, col 6"), std::string::npos);
    }
}

TEST(ExprParser, ParsesCollectionListsWithObjectsAndTrailingComma) {
    const auto value = Parse("[Training, TWeapon { Name = \"Sword\"; Features = [Keen]; },]");
    ASSERT_EQ(value.Kind, ENodeKind::ListLiteral);
    ASSERT_EQ(value.Args.size(), 2u);
    EXPECT_EQ(value.Args[0].Text, "Training");
    ASSERT_EQ(value.Args[1].Kind, ENodeKind::ObjectInit);
    ASSERT_EQ(value.Args[1].Fields.size(), 2u);
    const auto& nested = value.Args[1].Fields[1].Value;
    ASSERT_EQ(nested.Kind, ENodeKind::ListLiteral);
    EXPECT_EQ(nested.Args[0].Text, "Keen");
    auto copy = value;
    copy.Args[1].Fields[1].Value.Args[0].Text = "Changed";
    EXPECT_EQ(nested.Args[0].Text, "Keen");
    EXPECT_TRUE(Parse("[]").Args.empty());
}

TEST(ExprParser, RetainsPostfixIndexingAfterListLiteral) {
    const auto value = Parse("[1, 2][0]");
    ASSERT_EQ(value.Kind, ENodeKind::Index);
    EXPECT_EQ(value.Lhs->Kind, ENodeKind::ListLiteral);
    EXPECT_EQ(value.Rhs->Text, "0");
}

TEST(ExprParser, RejectsMalformedListLiterals) {
    for (const std::string source : {"[", "[1", "[1 2]", "[,]", "[1,,]"}) {
        EXPECT_THROW(Parse(source), std::runtime_error) << source;
    }
}

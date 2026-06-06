#include <gtest/gtest.h>

#include <ttrpg/analyze.h>
#include <ttrpg/parser.h>
#include <ttrpg/schema_ast.h>

#include <expr/ast.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

TSchemaModule ParseModule(const std::string& src) {
    return TParser(Tokenize(src)).Parse();
}

TClassDecl ParseClass(const std::string& src) {
    return ParseModule(src).Classes.at(0);
}

std::unordered_map<std::string, const TClassDecl*> ClassMap(const TSchemaModule& m) {
    std::unordered_map<std::string, const TClassDecl*> classes;
    for (const auto& c : m.Classes) {
        classes[c.Name] = &c;
    }
    return classes;
}

size_t PosOf(const std::vector<size_t>& order, size_t field) {
    return static_cast<size_t>(
        std::find(order.begin(), order.end(), field) - order.begin());
}

}

TEST(AnalyzeTest, OrdersRefsBeforeDependent) {
    TClassDecl c = ParseClass(
        "class TC {\n"
        "    int RaceHp;\n"
        "    int Level;\n"
        "    int ClassHp;\n"
        "    BoundedQuantity Hp = RaceHp + Level * ClassHp;\n"
        "}\n");
    std::vector<size_t> order = FieldInitOrder(c, {});
    ASSERT_EQ(order.size(), 4u);
    EXPECT_LT(PosOf(order, 0), PosOf(order, 3));
    EXPECT_LT(PosOf(order, 1), PosOf(order, 3));
    EXPECT_LT(PosOf(order, 2), PosOf(order, 3));
}

TEST(AnalyzeTest, OrdersForwardReference) {
    TClassDecl c = ParseClass(
        "class TC {\n"
        "    BoundedQuantity Hp = RaceHp + Level * ClassHp;\n"
        "    int RaceHp;\n"
        "    int Level;\n"
        "    int ClassHp;\n"
        "}\n");
    std::vector<size_t> order = FieldInitOrder(c, {});
    ASSERT_EQ(order.size(), 4u);
    EXPECT_LT(PosOf(order, 1), PosOf(order, 0));
    EXPECT_LT(PosOf(order, 2), PosOf(order, 0));
    EXPECT_LT(PosOf(order, 3), PosOf(order, 0));
}

TEST(AnalyzeTest, ConstantDefaultsAreNotComputed) {
    TClassDecl c = ParseClass(
        "class TC {\n"
        "    int A = 0;\n"
        "    int B = 5;\n"
        "}\n");
    std::vector<size_t> order = FieldInitOrder(c, {});
    EXPECT_EQ(order, (std::vector<size_t>{0, 1}));
}

TEST(AnalyzeTest, CycleThrows) {
    TClassDecl c = ParseClass("class A { int x = y; int y = x; }\n");
    EXPECT_THROW(FieldInitOrder(c, {}), std::runtime_error);
}

TEST(AnalyzeTest, UnknownFieldInComputedExprThrows) {
    TClassDecl c = ParseClass("class A { int y; int x = y + z; }\n");
    EXPECT_THROW(FieldInitOrder(c, {}), std::runtime_error);
}

TEST(AnalyzeTest, NonIntReferenceThrows) {
    TClassDecl c = ParseClass("class A { BoundedQuantity B; int X = B + 1; }\n");
    EXPECT_THROW(FieldInitOrder(c, {}), std::runtime_error);
}

TEST(AnalyzeTest, ComputedFieldMustBeIntOrBoundedQuantity) {
    TClassDecl c = ParseClass("class A { int N; string S = N + N; }\n");
    EXPECT_THROW(FieldInitOrder(c, {}), std::runtime_error);
}

TEST(AnalyzeTest, CallAndComparisonAreRejected) {
    EXPECT_THROW(FieldInitOrder(ParseClass("class A { int x = f(1); }\n"), {}),
                 std::runtime_error);
    EXPECT_THROW(FieldInitOrder(ParseClass("class A { int a; int x = a > 1; }\n"), {}),
                 std::runtime_error);
}

TEST(AnalyzeTest, InitExprToCppMapsRefsAndMembers) {
    TClassDecl c = ParseClass("class A { int A; int B; int X = A + B * 2; }\n");
    const expr::TExprNode& e = c.Fields.at(2).Init.value();
    EXPECT_EQ(InitExprToCpp(e), "(r.A_ + (r.B_ * 2))");
}

TEST(AnalyzeTest, MemberAccessValidatesAndLowers) {
    TSchemaModule m = ParseModule(
        "class TPart { int Bonus; }\n"
        "class TC {\n"
        "    int Base;\n"
        "    TPart Part;\n"
        "    int X = Part.Bonus + Base;\n"
        "}\n");
    const TClassDecl& c = m.Classes.at(1);
    std::vector<size_t> order = FieldInitOrder(c, ClassMap(m));
    ASSERT_EQ(order.size(), 3u);
    EXPECT_LT(PosOf(order, 0), PosOf(order, 2));
    EXPECT_LT(PosOf(order, 1), PosOf(order, 2));
    EXPECT_EQ(InitExprToCpp(c.Fields.at(2).Init.value()), "(r.Part_.Bonus() + r.Base_)");
}

TEST(AnalyzeTest, MemberAccessOnNonClassThrows) {
    TSchemaModule m = ParseModule("class TC { int Base; int X = Base.Bonus; }\n");
    EXPECT_THROW(FieldInitOrder(m.Classes.at(0), ClassMap(m)), std::runtime_error);
}

TEST(AnalyzeTest, MemberAccessUnknownMemberThrows) {
    TSchemaModule m = ParseModule(
        "class TPart { int Bonus; }\n"
        "class TC { TPart Part; int X = Part.Missing; }\n");
    EXPECT_THROW(FieldInitOrder(m.Classes.at(1), ClassMap(m)), std::runtime_error);
}

TEST(AnalyzeTest, DeriveExcludedFromInitOrder) {
    TClassDecl c = ParseClass(
        "class TC { int Value; derive int Modifier = (Value - 10) / 2; }\n");
    std::vector<size_t> order = FieldInitOrder(c, {});
    EXPECT_EQ(order, (std::vector<size_t>{0}));
}

TEST(AnalyzeTest, DeriveGetterLowersWithSelfPrefix) {
    TClassDecl c = ParseClass(
        "class TC { int Value; derive int Modifier = (Value - 10) / 2; }\n");
    EXPECT_EQ(InitExprToCpp(c.Fields.at(1).Init.value(), ""), "((Value_ - 10) / 2)");
}

TEST(AnalyzeTest, DeriveTypeMismatchThrows) {
    TClassDecl c = ParseClass("class TC { int Value; derive bool Flag = Value - 1; }\n");
    EXPECT_THROW(FieldInitOrder(c, {}), std::runtime_error);
}

TEST(AnalyzeTest, BareReferenceToDeriveFieldThrows) {
    TClassDecl c = ParseClass(
        "class TC { int Value; derive int A = Value; derive int B = A; }\n");
    EXPECT_THROW(FieldInitOrder(c, {}), std::runtime_error);
}

TEST(AnalyzeTest, MemberAccessIntoDeriveFieldLowers) {
    TSchemaModule m = ParseModule(
        "class TInner { int X; derive int Y = X * 2; }\n"
        "class TC { int Base; TInner Inner; int Z = Inner.Y + Base; }\n");
    const TClassDecl& c = m.Classes.at(1);
    std::vector<size_t> order = FieldInitOrder(c, ClassMap(m));
    ASSERT_EQ(order.size(), 3u);
    EXPECT_LT(PosOf(order, 1), PosOf(order, 2));
    EXPECT_EQ(InitExprToCpp(c.Fields.at(2).Init.value()), "(r.Inner_.Y() + r.Base_)");
}

TEST(AnalyzeTest, TwoLevelMemberChainValidatesAndLowers) {
    TSchemaModule m = ParseModule(
        "class TLeaf { int X; }\n"
        "class TMid { TLeaf Leaf; }\n"
        "class TC { TMid Mid; int Z = Mid.Leaf.X; }\n");
    const TClassDecl& c = m.Classes.at(2);
    std::vector<size_t> order = FieldInitOrder(c, ClassMap(m));
    ASSERT_EQ(order.size(), 2u);
    EXPECT_LT(PosOf(order, 0), PosOf(order, 1));
    EXPECT_EQ(InitExprToCpp(c.Fields.at(1).Init.value()), "r.Mid_.Leaf().X()");
}

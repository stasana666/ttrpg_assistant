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

// A class-name -> decl table over a module, for member-access validation.
// The TClassDecl pointers stay valid as long as the module does.
std::unordered_map<std::string, const TClassDecl*> ClassMap(const TSchemaModule& m) {
    std::unordered_map<std::string, const TClassDecl*> classes;
    for (const auto& c : m.Classes) {
        classes[c.Name] = &c;
    }
    return classes;
}

// Position of field index `field` within an evaluation order.
size_t PosOf(const std::vector<size_t>& order, size_t field) {
    return static_cast<size_t>(
        std::find(order.begin(), order.end(), field) - order.begin());
}

}  // namespace

// Computed field declared after its references: it must be evaluated last.
TEST(AnalyzeTest, OrdersRefsBeforeDependent) {
    TClassDecl c = ParseClass(
        "class TC {\n"
        "    int RaceHp;\n"          // 0
        "    int Level;\n"           // 1
        "    int ClassHp;\n"         // 2
        "    BoundedQuantity Hp = RaceHp + Level * ClassHp;\n"  // 3 (computed)
        "}\n");
    std::vector<size_t> order = FieldInitOrder(c, {});
    ASSERT_EQ(order.size(), 4u);
    EXPECT_LT(PosOf(order, 0), PosOf(order, 3));
    EXPECT_LT(PosOf(order, 1), PosOf(order, 3));
    EXPECT_LT(PosOf(order, 2), PosOf(order, 3));
}

// Same dependency graph, but the computed field is declared *first*. Order is
// driven by dependencies, not declaration order.
TEST(AnalyzeTest, OrdersForwardReference) {
    TClassDecl c = ParseClass(
        "class TC {\n"
        "    BoundedQuantity Hp = RaceHp + Level * ClassHp;\n"  // 0 (computed)
        "    int RaceHp;\n"          // 1
        "    int Level;\n"           // 2
        "    int ClassHp;\n"         // 3
        "}\n");
    std::vector<size_t> order = FieldInitOrder(c, {});
    ASSERT_EQ(order.size(), 4u);
    EXPECT_LT(PosOf(order, 1), PosOf(order, 0));
    EXPECT_LT(PosOf(order, 2), PosOf(order, 0));
    EXPECT_LT(PosOf(order, 3), PosOf(order, 0));
}

// Constant defaults create no dependency edges; declaration order is preserved.
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
    // `y` is a field, `z` is not -> a computed expression referencing an unknown.
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

// Member access: `Part.Bonus` lowers to a getter call and creates a dependency
// on the `Part` field.
TEST(AnalyzeTest, MemberAccessValidatesAndLowers) {
    TSchemaModule m = ParseModule(
        "class TPart { int Bonus; }\n"
        "class TC {\n"
        "    int Base;\n"            // 0
        "    TPart Part;\n"          // 1
        "    int X = Part.Bonus + Base;\n"  // 2 (computed, member access)
        "}\n");
    const TClassDecl& c = m.Classes.at(1);
    std::vector<size_t> order = FieldInitOrder(c, ClassMap(m));
    ASSERT_EQ(order.size(), 3u);
    EXPECT_LT(PosOf(order, 0), PosOf(order, 2));  // Base before X
    EXPECT_LT(PosOf(order, 1), PosOf(order, 2));  // Part before X
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

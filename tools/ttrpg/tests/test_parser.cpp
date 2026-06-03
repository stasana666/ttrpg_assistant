#include <gtest/gtest.h>

#include <ttrpg/module.h>
#include <ttrpg/parser.h>
#include <ttrpg/schema_ast.h>

#include <expr/ast.h>

#include <string>
#include <unordered_map>

#ifndef TTRPG_FIXTURES_DIR
#error "TTRPG_FIXTURES_DIR must be defined by the build"
#endif

namespace {

TSchemaModule ParseSrc(const std::string& src) {
    return TParser(Tokenize(src)).Parse();
}

const std::string kFixtures = TTRPG_FIXTURES_DIR;

}  // namespace

TEST(ParserTest, InitExprPrecedence) {
    TSchemaModule m = ParseSrc(
        "class TC { int A; int B; int C; int X = A + B * C; }\n");
    const TClassDecl& c = m.Classes.at(0);
    const expr::TExprNode& x = c.Fields.at(3).Init.value();
    // '+' is the root; '*' binds tighter and sits on the right.
    ASSERT_EQ(x.Kind, expr::ENodeKind::Binary);
    EXPECT_EQ(x.BinOp, expr::EBinaryOp::Add);
    EXPECT_EQ(x.Lhs->Kind, expr::ENodeKind::Var);
    EXPECT_EQ(x.Lhs->Text, "A");
    ASSERT_EQ(x.Rhs->Kind, expr::ENodeKind::Binary);
    EXPECT_EQ(x.Rhs->BinOp, expr::EBinaryOp::Mul);
    EXPECT_EQ(x.Rhs->Lhs->Text, "B");
    EXPECT_EQ(x.Rhs->Rhs->Text, "C");
}

TEST(ParserTest, ConstantInitsAreSingleTokens) {
    TSchemaModule m = ParseSrc(
        "class TC { int A = 0; int B = max_int; int C = -2; }\n");
    const TClassDecl& c = m.Classes.at(0);
    EXPECT_EQ(c.Fields.at(0).Init->Kind, expr::ENodeKind::IntLiteral);
    EXPECT_EQ(c.Fields.at(0).Init->Text, "0");
    EXPECT_EQ(c.Fields.at(1).Init->Kind, expr::ENodeKind::Var);
    EXPECT_EQ(c.Fields.at(1).Init->Text, "max_int");
    EXPECT_EQ(c.Fields.at(2).Init->Kind, expr::ENodeKind::IntLiteral);
    EXPECT_EQ(c.Fields.at(2).Init->Text, "-2");
}

TEST(ParserTest, EnumAndClass) {
    TSchemaModule m = ParseSrc(
        "enum EColor { Red, Green, }\n"
        "class TThing { string Name; int Count = 3; set<EColor> Tags; }\n");

    ASSERT_EQ(m.Enums.size(), 1u);
    EXPECT_EQ(m.Enums[0].Name, "EColor");
    EXPECT_EQ(m.Enums[0].Values.size(), 2u);

    ASSERT_EQ(m.Classes.size(), 1u);
    const TClassDecl& c = m.Classes[0];
    ASSERT_EQ(c.Fields.size(), 3u);
    EXPECT_EQ(c.Fields[0].TypeName, "string");
    EXPECT_FALSE(c.Fields[0].Init.has_value());
    EXPECT_EQ(c.Fields[1].Init->Text, "3");
    EXPECT_EQ(c.Fields[2].Container, EContainer::Set);
    EXPECT_EQ(c.Fields[2].TypeName, "EColor");
}

TEST(ParserTest, VariantThreeAlternativeForms) {
    TSchemaModule m = ParseSrc(
        "variant TEffect {\n"
        "    Stun;\n"
        "    Burn { int Damage; }\n"
        "    Zone { int Radius = 1; EColor Color; }\n"
        "}\n");

    ASSERT_EQ(m.Variants.size(), 1u);
    const TVariantDecl& v = m.Variants[0];
    EXPECT_EQ(v.Name, "TEffect");
    ASSERT_EQ(v.Alternatives.size(), 3u);

    EXPECT_EQ(v.Alternatives[0].Name, "Stun");
    EXPECT_TRUE(v.Alternatives[0].Fields.empty());                 // flag

    EXPECT_EQ(v.Alternatives[1].Name, "Burn");
    ASSERT_EQ(v.Alternatives[1].Fields.size(), 1u);                // single-field
    EXPECT_EQ(v.Alternatives[1].Fields[0].TypeName, "int");

    EXPECT_EQ(v.Alternatives[2].Name, "Zone");
    ASSERT_EQ(v.Alternatives[2].Fields.size(), 2u);                // multi-field + default
    EXPECT_EQ(v.Alternatives[2].Fields[0].Init->Text, "1");
}

TEST(ParserTest, ImportsRecorded) {
    TSchemaModule m = ParseSrc("import \"dice.ttrpg\";\nenum E { A, }\n");
    ASSERT_EQ(m.Imports.size(), 1u);
    EXPECT_EQ(m.Imports[0], "dice.ttrpg");
}

TEST(ParserTest, DuplicateTypeThrows) {
    TSchemaModule m = ParseSrc("enum EColor { Red, }\nclass EColor { int X; }\n");
    std::unordered_map<std::string, TTypeInfo> table;
    EXPECT_THROW(RegisterModuleSymbols("dup", m, table), std::runtime_error);
}

TEST(ParserTest, DuplicateAlternativeThrows) {
    EXPECT_THROW(ParseSrc("variant V { A; A; }\n"), std::runtime_error);
}

TEST(ParserTest, CircularImportThrows) {
    EXPECT_THROW(LoadAll(kFixtures + "/cycle_a.ttrpg"), std::runtime_error);
}

TEST(ParserTest, LoadAllBuildsSymbolTable) {
    TLoadedSchemas loaded = LoadAll(kFixtures + "/variant.ttrpg");
    EXPECT_EQ(loaded.SymbolTable.at("EColor").Kind, ETypeKind::Enum);
    EXPECT_EQ(loaded.SymbolTable.at("TEffect").Kind, ETypeKind::Variant);
    EXPECT_EQ(loaded.SymbolTable.at("TSpell").Kind, ETypeKind::Class);
}

#include <gtest/gtest.h>

#include <ttrpg/module.h>
#include <ttrpg/parser.h>
#include <ttrpg/schema_ast.h>

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
    EXPECT_FALSE(c.Fields[0].DefaultExpr.has_value());
    EXPECT_EQ(c.Fields[1].DefaultExpr.value(), "3");
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
    EXPECT_EQ(v.Alternatives[2].Fields[0].DefaultExpr.value(), "1");
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

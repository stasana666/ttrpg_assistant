#include <gtest/gtest.h>

#include <ttrpg/conventions.h>
#include <ttrpg/module.h>
#include <ttrpg/schema_ast.h>

#include <string>
#include <unordered_map>

namespace {

TFieldDecl Field(const std::string& type, const std::string& name,
                 EContainer container = EContainer::None) {
    TFieldDecl f;
    f.TypeName = type;
    f.Name = name;
    f.Container = container;
    return f;
}

std::unordered_map<std::string, TTypeInfo> Symbols() {
    return {
        {"EColor", {"basic", ETypeKind::Enum}},
        {"TThing", {"basic", ETypeKind::Class}},
        {"TEffect", {"variant", ETypeKind::Variant}},
    };
}

}

TEST(ConventionsTest, PascalToSnake) {
    EXPECT_EQ(PascalToSnake("FooBar"), "foo_bar");
    EXPECT_EQ(PascalToSnake("Name"), "name");
    EXPECT_EQ(PascalToSnake("BaseDiceSize"), "base_dice_size");
    EXPECT_EQ(PascalToSnake("X"), "x");
}

TEST(ConventionsTest, VariantNaming) {
    EXPECT_EQ(VariantKindEnum("TWeaponTrait"), "EWeaponTraitKind");
    EXPECT_EQ(VariantKindEnum("TEffect"), "EEffectKind");
    EXPECT_EQ(VariantKindEnum("Foo"), "EFooKind");
    EXPECT_EQ(PayloadStructName("TWeaponTrait", "Fatal"), "TWeaponTraitFatal");
}

TEST(ConventionsTest, FieldKindResolvesViaSymbolTable) {
    auto sym = Symbols();
    EXPECT_EQ(FieldKindOf(Field("int", "X"), sym), EFieldKind::Primitive);
    EXPECT_EQ(FieldKindOf(Field("string", "X"), sym), EFieldKind::Primitive);
    EXPECT_EQ(FieldKindOf(Field("EColor", "X"), sym), EFieldKind::Enum);
    EXPECT_EQ(FieldKindOf(Field("TThing", "X"), sym), EFieldKind::Class);
    EXPECT_EQ(FieldKindOf(Field("TEffect", "X"), sym), EFieldKind::Variant);
}

TEST(ConventionsTest, CppMemberTypeForSets) {
    auto sym = Symbols();
    EXPECT_EQ(CppMemberType(Field("EColor", "Tags", EContainer::Set), sym),
              "std::set<EColor>");
    EXPECT_EQ(CppMemberType(Field("TEffect", "Effects", EContainer::Set), sym),
              "TVariantMap<EEffectKind, TEffect>");
    EXPECT_EQ(CppMemberType(Field("string", "Name"), sym), "std::string");
}

TEST(ConventionsTest, BoundedQuantityIsBuiltinValueType) {
    auto sym = Symbols();
    EXPECT_TRUE(IsBuiltinBoundedQuantity("BoundedQuantity"));
    EXPECT_FALSE(IsBuiltinBoundedQuantity("int"));
    EXPECT_EQ(FieldKindOf(Field("BoundedQuantity", "Hp"), sym), EFieldKind::BoundedQuantity);
    EXPECT_EQ(CppMemberType(Field("BoundedQuantity", "Hp"), sym), "TBoundedQuantity");
}

TEST(ConventionsTest, DefaultExprToCpp) {
    EXPECT_EQ(DefaultExprToCpp("max_int", "int"), "std::numeric_limits<int>::max()");
    EXPECT_EQ(DefaultExprToCpp("min_int", "int"), "std::numeric_limits<int>::min()");
    EXPECT_EQ(DefaultExprToCpp("5", "int"), "5");
    EXPECT_EQ(DefaultExprToCpp("-2", "int"), "-2");
    EXPECT_EQ(DefaultExprToCpp("true", "bool"), "true");
    EXPECT_EQ(DefaultExprToCpp("Red", "EColor"), "EColor::Red");
}

TEST(ConventionsTest, BuiltinPrimitives) {
    EXPECT_TRUE(IsBuiltinPrimitive("int"));
    EXPECT_TRUE(IsBuiltinPrimitive("bool"));
    EXPECT_TRUE(IsBuiltinPrimitive("string"));
    EXPECT_FALSE(IsBuiltinPrimitive("EColor"));
}

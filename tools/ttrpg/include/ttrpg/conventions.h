#pragma once


#include <ttrpg/module.h>
#include <ttrpg/schema_ast.h>

#include <string>
#include <unordered_map>

std::string PascalToSnake(const std::string& s);

bool IsBuiltinInt(const std::string& t);
bool IsBuiltinBool(const std::string& t);
bool IsBuiltinString(const std::string& t);
bool IsBuiltinPrimitive(const std::string& t);

bool IsBuiltinBoundedQuantity(const std::string& t);
bool IsBuiltinResource(const std::string& t);

std::string CppTypeFor(const std::string& schemaType);

enum class EFieldKind {
    Primitive,
    Enum,
    Class,
    Variant,
    BoundedQuantity,
    Resource,
};

EFieldKind FieldKindOf(const TFieldDecl& f,
                       const std::unordered_map<std::string, TTypeInfo>& symbols);

bool IsDslSupported(const TFieldDecl& f);

std::string DefaultExprToCpp(const std::string& expr, const std::string& schemaType);

std::string CppPrimitiveType(const TFieldDecl& f);

std::string ScalarParseExpr(const TFieldDecl& f,
                            EFieldKind kind,
                            const std::string& valExpr);

std::string LoadFieldCall(const TFieldDecl& f,
                          EFieldKind kind,
                          const std::string& jsonKey);

std::string DeriveSiblingInclude(const std::string& primaryOutH, const std::string& stem);

std::string VariantKindEnum(const std::string& variantName);

std::string PayloadStructName(const std::string& variantName, const std::string& altName);

bool IsVariantType(const std::string& typeName,
                   const std::unordered_map<std::string, TTypeInfo>& symbols);

std::string CppMemberType(const TFieldDecl& f,
                          const std::unordered_map<std::string, TTypeInfo>& symbols);

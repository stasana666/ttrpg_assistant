#pragma once

// The semantic / naming layer: the conventions that map a schema field to its
// C++ spelling (member type, JSON key, parse expression, field kind, etc.).
// This is the intermediate layer between the schema AST and the emitters --
// the emitters decide *structure*, these helpers decide *names and types*.

#include <ttrpg/module.h>
#include <ttrpg/schema_ast.h>

#include <string>
#include <unordered_map>

std::string PascalToSnake(const std::string& s);

bool IsBuiltinInt(const std::string& t);
bool IsBuiltinBool(const std::string& t);
bool IsBuiltinString(const std::string& t);
bool IsBuiltinPrimitive(const std::string& t);

std::string CppTypeFor(const std::string& schemaType);

// Per-field kind, derived from the (cross-file) symbol table.
enum class EFieldKind {
    Primitive,  // int / bool / string
    Enum,       // schema-declared enum
    Class,      // schema-declared class -> loads via factory, owned in AST
    Variant,    // schema-declared variant -> loads via FromJson, owned in AST
};

EFieldKind FieldKindOf(const TFieldDecl& f,
                       const std::unordered_map<std::string, TTypeInfo>& symbols);

bool IsDslSupported(const TFieldDecl& f);

std::string DefaultExprToCpp(const std::string& expr, const std::string& schemaType);

std::string CppPrimitiveType(const TFieldDecl& f);

// Parse a single scalar value out of an arbitrary json-value C++ expression
// `valExpr` (e.g. "val", or "j.at(\"key\")"). Default handling is the caller's
// job. Shared by class-field loading and variant-payload loading.
std::string ScalarParseExpr(const TFieldDecl& f,
                            EFieldKind kind,
                            const std::string& valExpr);

std::string LoadFieldCall(const TFieldDecl& f,
                          EFieldKind kind,
                          const std::string& jsonKey);

// Derive a header include path of the form "pf2e_engine/<dir>/<stem>.h"
// from the primary --out-h argument and an arbitrary stem.
std::string DeriveSiblingInclude(const std::string& primaryOutH, const std::string& stem);

// The kind/discriminant enum generated for a variant. `TWeaponTrait` ->
// `EWeaponTraitKind`: strip a leading 'T', prefix 'E', suffix 'Kind'.
std::string VariantKindEnum(const std::string& variantName);

// The payload struct for one alternative: variant name + alternative name.
std::string PayloadStructName(const std::string& variantName, const std::string& altName);

bool IsVariantType(const std::string& typeName,
                   const std::unordered_map<std::string, TTypeInfo>& symbols);

std::string CppMemberType(const TFieldDecl& f,
                          const std::unordered_map<std::string, TTypeInfo>& symbols);

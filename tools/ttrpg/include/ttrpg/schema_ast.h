#pragma once


#include <expr/ast.h>

#include <optional>
#include <string>
#include <vector>

struct TEnumDecl {
    std::string Name;
    std::vector<std::string> Values;
};

enum class EContainer {
    None,
    Set,
};

struct TFieldDecl {
    std::string TypeName;
    EContainer Container = EContainer::None;
    std::string Name;
    std::optional<expr::TExprNode> Init;
};

struct TClassDecl {
    std::string Name;
    std::vector<TFieldDecl> Fields;
};

struct TVariantAlt {
    std::string Name;
    std::vector<TFieldDecl> Fields;
};

struct TVariantDecl {
    std::string Name;
    std::vector<TVariantAlt> Alternatives;
};

struct TSchemaModule {
    std::vector<std::string> Imports;
    std::vector<TEnumDecl> Enums;
    std::vector<TVariantDecl> Variants;
    std::vector<TClassDecl> Classes;
};

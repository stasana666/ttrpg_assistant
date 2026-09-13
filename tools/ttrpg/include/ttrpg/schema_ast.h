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
    Collection,
    Map,
};

struct TFieldDecl {
    std::string TypeName;
    std::string ValueTypeName;
    EContainer Container = EContainer::None;
    std::string Name;
    std::optional<expr::TExprNode> Init;
    bool Derived = false;
};

struct TClassDecl {
    std::string Name;
    std::vector<TFieldDecl> Fields;
    bool GameEntity = false;
    struct TParameter {
        std::string TypeName;
        std::string Name;
    };
    struct TStatement {
        enum class EKind { Let, Require, Assign, AddAssign, SubAssign, Call, Return, If };
        EKind Kind = EKind::Call;
        std::string Name;
        std::optional<expr::TExprNode> Target;
        std::optional<expr::TExprNode> Value;
        std::vector<TStatement> Body;
        std::vector<TStatement> ElseBody;
        parse::TSourceLocation Location;
    };
    struct TMethod {
        std::string ReturnType;
        std::string Name;
        std::vector<TParameter> Parameters;
        std::vector<TStatement> Body;
        bool Query = false;
        parse::TSourceLocation Location;
    };
    std::vector<TMethod> Methods;
};

struct TInstanceDecl {
    std::string TypeName;
    std::string Name;
    expr::TExprNode Value;
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
    std::vector<TInstanceDecl> Instances;
};

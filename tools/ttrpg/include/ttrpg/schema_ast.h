#pragma once

// The schema AST: the parsed, in-memory representation of one `.ttrpg` module,
// before any C++ is emitted. Produced by the parser, consumed by the emitters.

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
    std::string TypeName;              // element type when Container != None
    EContainer Container = EContainer::None;
    std::string Name;
    // The `= ...` initializer, if any, parsed by the shared expression
    // front-end. A bare literal / non-field identifier is a constant default;
    // an expression referencing sibling fields (or any member/binary node) is a
    // computed default -- see analyze.h.
    std::optional<expr::TExprNode> Init;
};

struct TClassDecl {
    std::string Name;
    std::vector<TFieldDecl> Fields;
};

// One alternative of a `variant`. A flag alternative has no fields; a
// parameterized alternative carries one or more fields (same syntax as class
// fields, including defaults).
struct TVariantAlt {
    std::string Name;
    std::vector<TFieldDecl> Fields;
};

struct TVariantDecl {
    std::string Name;
    std::vector<TVariantAlt> Alternatives;
};

struct TSchemaModule {
    std::vector<std::string> Imports;       // raw paths from import directives
    std::vector<TEnumDecl> Enums;
    std::vector<TVariantDecl> Variants;
    std::vector<TClassDecl> Classes;
};

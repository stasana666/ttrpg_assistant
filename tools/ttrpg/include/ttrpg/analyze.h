#pragma once

// Semantic analysis of field initializers (`= ...`, parsed into expr::TExprNode
// by the shared front-end). Distinguishes a constant default (literal / enum
// value / max_int -- unchanged JSON-overridable default) from a *computed*
// default (an expression referencing sibling fields, possibly via member
// access), orders fields so a computed field is assigned after the fields it
// references, and rejects cycles / bad references / codegen-unsupported nodes at
// codegen time.

#include <ttrpg/schema_ast.h>

#include <expr/ast.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// True if `e` is a computed default: any member/call/unary/binary node, or a
// bare identifier that names one of `fieldNames`. A plain literal, or an
// identifier that is not a sibling field (enum value, max_int, ...), is a
// constant default.
bool InitIsComputed(const expr::TExprNode& e, const std::unordered_set<std::string>& fieldNames);

// Evaluation order for a class's fields such that every computed field comes
// after the fields its initializer references. `classes` maps a class type name
// to its declaration (for validating member access like `Race.Hitpoints`).
// Throws std::runtime_error on: a dependency cycle; a computed field whose type
// is not int / BoundedQuantity; an initializer referencing an unknown / non-int
// field; a member access whose base is not a class field or whose member is not
// an int field; or a codegen-unsupported node (call, comparison/logical, `$`-var).
std::vector<std::size_t> FieldInitOrder(
    const TClassDecl& c,
    const std::unordered_map<std::string, const TClassDecl*>& classes);

// Lower a computed initializer expression to C++: integer literals verbatim, a
// field reference `Foo` to its member `r.Foo_`, member access `Foo.Bar` to the
// getter call `r.Foo_.Bar()`, binary arithmetic fully parenthesized.
std::string InitExprToCpp(const expr::TExprNode& e);

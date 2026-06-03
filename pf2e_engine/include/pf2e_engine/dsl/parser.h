#pragma once

#include <pf2e_engine/dsl/expression.h>

#include <memory>
#include <string_view>

// Parses a DSL source string into an evaluable expression. Lexing and parsing
// are delegated to the shared expression front-end (tools/common/expr); this
// returns a TDslExpression wrapping the resulting AST. Throws std::runtime_error
// on lexer/parser errors. The grammar (a superset shared with the codegen)
// supports arithmetic, comparison, logical, member access (`$x.prop`), function
// calls, integer literals, and parentheses.
std::unique_ptr<TDslExpression> ParseDsl(std::string_view src);

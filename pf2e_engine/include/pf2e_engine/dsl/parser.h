#pragma once

#include <pf2e_engine/dsl/expression.h>

#include <memory>
#include <string_view>

// Parses a DSL source string into an evaluable expression tree. Throws
// std::runtime_error on lexer/parser errors. Grammar (boolean + comparison +
// property access + function calls; arithmetic intentionally deferred):
//
//   expression  := logical_or
//   logical_or  := logical_and ('||' logical_and)*
//   logical_and := equality   ('&&' equality)*
//   equality    := comparison (('=='|'!=') comparison)*
//   comparison  := unary      (('>='|'<='|'>'|'<') unary)*
//   unary       := '!' unary | postfix
//   postfix     := primary ('.' identifier)*
//   primary     := number | '$' identifier | identifier '(' args? ')' | '(' expression ')'
std::unique_ptr<IDslExpression> ParseDsl(std::string_view src);

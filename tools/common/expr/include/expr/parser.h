#pragma once

// Recursive-descent parser for the shared expression grammar. Throws
// std::runtime_error on lexer/parser errors. The grammar is a superset; each
// backend (codegen lowerer, DSL evaluator) supports its own subset of nodes.
//
//   expression := logical_or
//   logical_or := logical_and ('||' logical_and)*
//   logical_and:= equality   ('&&' equality)*
//   equality   := comparison (('=='|'!=') comparison)*
//   comparison := add_sub    (('>='|'<='|'>'|'<') add_sub)*
//   add_sub    := mul_div    (('+'|'-') mul_div)*
//   mul_div    := unary      (('*'|'/') unary)*
//   unary      := '!' unary | postfix
//   postfix    := primary ('.' identifier)*
//   primary    := int | '$' identifier | identifier | identifier '(' args? ')' | '(' expression ')'

#include <expr/ast.h>

#include <string>

namespace expr {

TExprNode Parse(const std::string& src);

}  // namespace expr

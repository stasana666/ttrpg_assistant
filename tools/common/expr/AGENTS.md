# Expression Front-End

## Purpose
- Shared expression lexer/parser/AST used by two backends: `.ttrpg` codegen lowers supported nodes to C++, and the engine runtime DSL evaluates the AST.

## Key Files
- `include/expr/ast.h`: backend-neutral `expr::TExprNode`, node kinds, unary ops, and binary ops.
- `src/lexer.cpp`: tokenizes arithmetic, logical/comparison, member access, calls, and `$` variables.
- `src/parser.cpp`: recursive-descent precedence parser.
- `tests/test_expr.cpp`: grammar and AST shape tests.

## Grammar
- Supports int literals, identifiers, `$name`, member chains, function calls, `!`, `+ - * /`, comparisons/equality, `&&`, `||`, and parentheses.
- The grammar is a superset; each backend decides what nodes it supports.

## Pitfalls
- Do not add backend-specific semantics here.
- Changes affect both generated schema defaults and runtime action expressions.
- Preserve source-location diagnostics from the `parse` layer.

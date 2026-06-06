# Common Parser Tools

## Purpose
- Shared lexical/parser infrastructure used by both `.ttrpg` codegen and engine runtime DSL.
- `parse` is header-only scanner/token-stream support.
- `expr` is a static library for expression lexing/parsing into `expr::TExprNode`.
- Read the nested `AGENTS.md` in `parse/` or `expr/` for local implementation details.

## Key Concepts
- `expr` supports integer literals, variables including `$vars`, member access, calls, unary `!`, arithmetic, comparisons, and logical operators.
- The AST is intentionally backend-agnostic; interpretation belongs in consumers such as `tools/ttrpg` or `pf2e_engine/src/dsl`.

## Build/Test
- Build and test expressions: `cmake --build build --target test_expr`.
- Tests are in `tools/common/expr/tests/test_expr.cpp`.

## Pitfalls
- Do not introduce PF2E or codegen-specific semantics into `expr`.
- Parser error text includes source locations from `parse::TScanner`/`TTokenStream`; preserve useful diagnostics.
- Changes here can affect both generated schema defaults and runtime action expressions.

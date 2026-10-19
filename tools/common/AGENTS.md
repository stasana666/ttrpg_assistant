# Common Parser Tools

Canonical project instructions. CLAUDE.md redirects here.

## Purpose
- Shared lexical/parser infrastructure used by the `.ttrpg` code generator.
- `parse` is header-only scanner/token-stream support.
- `expr` is a static library for expression lexing/parsing into `expr::TExprNode`.
- Read the nested `AGENTS.md` in `parse/` or `expr/` for local implementation details.

## Key Concepts
- `expr` supports integer literals, variables including `@self`, member access, calls, unary `!`, arithmetic, comparisons, and logical operators.
- The AST is intentionally backend-agnostic; interpretation belongs in `tools/ttrpg`.

## Build/Test
- Build and test expressions: `cmake --build build --target test_expr`.
- Tests are in `tools/common/expr/tests/test_expr.cpp`.

## Pitfalls
- Do not introduce PF2E or codegen-specific semantics into `expr`.
- Parser error text includes source locations from `parse::TScanner`/`TTokenStream`; preserve useful diagnostics.
- Changes here affect every expression the `.ttrpg` generator parses.

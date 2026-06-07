# Shared expression front-end (`expr`)

Canonical documentation file. AGENTS.md must remain semantically equivalent.

One lexer + parser + backend-agnostic AST shared by two backends:

- the **`.ttrpg` code generator** ([tools/ttrpg/](../../ttrpg/)), which **lowers**
  the AST to C++ (used for computed-default expressions); and
- the **runtime DSL** ([pf2e_engine/src/dsl/](../../../pf2e_engine/src/dsl/)), which
  **evaluates** the AST against the engine's property/function registries.

It references no engine or codegen types — each backend interprets the nodes it
supports and rejects the rest. It builds on the lower, grammar-agnostic
[tools/common/parse/](../parse/) layer (scanner + token-stream cursor).

## AST (`ast.h`)

`expr::TExprNode` is a single struct discriminated by `ENodeKind`
(`IntLiteral`, `Var`, `Member`, `Call`, `Unary`, `Binary`), with `EBinaryOp`
(arithmetic `Add/Sub/Mul/Div`, logical `Or/And`, comparison/equality
`Eq/Ne/Lt/Le/Gt/Ge`) and `EUnaryOp` (`Not`). `Var` carries `HasDollar` recording
whether it was written `$name`. The struct is intentionally backend-neutral; a
backend that doesn't support, say, calls or `$`-vars rejects them at lowering/eval
time.

## Lexer (`lexer.h`)

`Tokenize(src)` produces `ETok` tokens — the **union** of what the DSL and codegen
need: arithmetic, comparison, logical, member access (`.`), calls, `$`-vars. Built
on `parse::TScanner`.

## Parser (`parser.h`)

`Parse(src)` is a recursive-descent parser; throws `std::runtime_error` on
lexer/parser errors. The grammar is a **superset** — each backend supports its own
subset of nodes:

```
expression := logical_or
logical_or := logical_and ('||' logical_and)*
logical_and:= equality   ('&&' equality)*
equality   := comparison (('=='|'!=') comparison)*
comparison := add_sub    (('>='|'<='|'>'|'<') add_sub)*
add_sub    := mul_div    (('+'|'-') mul_div)*
mul_div    := unary      (('*'|'/') unary)*
unary      := '!' unary | postfix
postfix    := primary ('.' identifier)*
primary    := int | '$' identifier | identifier | identifier '(' args? ')' | '(' expression ')'
```

Backend subsets, for reference: the DSL evaluator supports the full set; the
codegen's computed-default lowerer rejects calls, `$`-vars, comparison, and
logical operators.

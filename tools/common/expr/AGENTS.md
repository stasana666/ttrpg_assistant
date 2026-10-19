# Shared expression front-end (`expr`)

Canonical project instructions. CLAUDE.md redirects here.

One lexer + parser + backend-agnostic AST. The `.ttrpg` code generator
([tools/ttrpg/](../../ttrpg/)) lowers it to C++.

It references no engine or codegen types — the backend interprets the nodes it
supports and rejects the rest. It builds on the lower, grammar-agnostic
[tools/common/parse/](../parse/) layer (scanner + token-stream cursor).

## AST (`ast.h`)

`expr::TExprNode` is discriminated by `ENodeKind`: `IntLiteral`, `Var`,
`Member`, `Call`, `Unary`, `Binary`, `ContextVar`, `ObjectInit`, `Index`, and
`StringLiteral` and `ListLiteral`. Existing enum values remain stable; new values are appended.
`EBinaryOp` covers arithmetic, logic, and comparisons. `EUnaryOp` has `Not`
and `Negate`. Nodes retain a `parse::TSourceLocation` in `Location`.

- A bare name is a `Var`; `@name` is a distinct `ContextVar`.
- Named `f(args)` preserves the existing `Call` representation: `Text = "f"`,
  `Lhs = nullptr`, and argument nodes in `Args`.
- Calls of member or other expressions use `Call.Lhs` for the callee expression
  and leave `Text` empty. `obj.Heal(1)` has a `Member` callee whose receiver is
  `obj`; repeated calls and index/member chains follow the same representation.
- `ObjectInit.Text` is the declared type name, or empty for a contextual
  initializer such as `{ Rank = Expert; }`. The backend supplies its type; the
  shared parser does not distinguish a class payload from a map initializer.
  `Fields` contains ordered
  `TFieldInitializer` values with `Name`, `Value`, and `Location`.
- `Index.Lhs` is the collection expression and `Index.Rhs` is its index.
- `StringLiteral.Text` stores the decoded string, without enclosing quotes.
- `ListLiteral.Args` stores ordered element expressions. Empty lists and a trailing comma are accepted; indexing a list uses the same postfix `Index` node as any other expression.

Types, method purity, valid assignment targets, context bindings, and supported
node kinds belong to each backend. Parsing a form does not authorize the runtime
DSL to execute it. Consumers must reject unsupported forms explicitly.

## Lexer (`lexer.h`)

`Tokenize(src)` produces `ETok` tokens for arithmetic, comparison, logic, member
access, calls, `@` variables,
object initializers, list literals, indexing, and strings. Built on `parse::TScanner`.
Strings support escaped quote, backslash, newline, carriage return, and tab;
unsupported escapes and literal newlines fail with source locations. Signed
integer literals retain their original single-token form when an operand can
begin; subtraction such as `Current-2` is tokenized independently of whitespace.

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
unary      := ('!' | '-') unary | postfix
postfix    := primary ('.' identifier | '(' args? ')' | '[' expression ']')*
primary    := int | string | '@' identifier | identifier
            | identifier? '{' (identifier '=' expression ';')* '}'
            | '[' (expression (',' expression)* ','?)? ']'
            | '(' expression ')'
args       := expression (',' expression)*
```

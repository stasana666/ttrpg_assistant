# Grammar-agnostic parsing primitives (`parse`)

Lowest layer shared by the expression front-end ([../expr/](../expr/)) and the
`.ttrpg` code generator. Knows nothing about any specific grammar.

## `TScanner` (`scanner.h`)

Character scanner with line/col tracking. Provides `Peek`/`PeekAt`/`Advance`,
`SkipWhitespace`, and lexing helpers `ScanIdent`, `ScanInteger`, `ScanQuotedString`,
plus `Try(char)`/`Try(string_view)` and the error helpers `Throw`/`ThrowAt`. Notes:

- `SkipLineComment(prefix)` — if the input at the current position starts with
  `prefix`, consumes up to (but not including) the next newline; returns whether a
  comment was consumed. This is how both the DSL lexer and the `.ttrpg` parser drop
  `//` comments.
- `ScanIdent` assumes the caller already checked `Peek()` is an identifier-start.
- `ScanInteger` scans an optional leading `-` then digits; the caller must have
  checked the head is a digit (or `-` followed by a digit).
- `ScanQuotedString` consumes the opening quote, scans to the matching closing
  quote, consumes that too. It throws on newline-in-string or EOF-in-string. **No
  escape handling** — add it when a use case appears.

## `TTokenStream` / `TToken` (`token_stream.h`)

Token-stream cursor templated on the consumer's token-kind enum, so each consumer
keeps its own kind set. `TToken<Kind>` carries `kind`, `text`, and a
`TSourceLocation`. The cursor offers `Peek`/`Consume`/`Advance`/`Match`/`Expect`,
with `Throw` reporting the offending token's text and location.

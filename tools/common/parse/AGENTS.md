# Parse Primitives

## Purpose
- Header-only, grammar-agnostic scanner and token-stream helpers used by `expr` and `.ttrpg` tooling.

## Key Files
- `include/parse/scanner.h`: `TScanner` with line/column tracking, whitespace/comment skipping, identifier/integer/string helpers, and location-aware errors.
- `include/parse/token_stream.h`: templated `TToken<Kind>` and `TTokenStream<Kind>` cursor.

## Conventions
- Keep this layer ignorant of `.ttrpg`, runtime DSL, PF2E, assistant, and analyzer concepts.
- Callers define their own token-kind enum; do not add consumer-specific token kinds here.

## Pitfalls
- `ScanIdent` and `ScanInteger` assume the caller already checked the first character.
- `ScanQuotedString` has no escape handling and throws on newline or EOF inside the string.
- `SkipLineComment("//")` consumes through the line but leaves the newline for whitespace handling.

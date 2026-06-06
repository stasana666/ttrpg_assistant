# Engine Common Utilities

## Purpose
- Low-level shared utilities for engine state, ids, events/channels, guarded values, continuations, AST support, and variant containers.

## Key Concepts
- `continuation.h` propagates `TSavepointStackUnwind` across `Then`, `While`, `ForEach`, and `ForEachOwned` so suspended gameplay can resume safely.
- `ForEach` ranges must outlive suspension; use `ForEachOwned` for temporary containers.
- `variant_map.h` backs generated `set<Variant>` fields with deterministic `std::map` iteration and typed `Has`/`Get<TAlt>()`.
- `overloaded` is used for exhaustive `std::visit` over generated variant payloads.

## Pitfalls
- Continuation helpers are exception-control-flow infrastructure; test resume/rollback behavior when changing them.
- Do not sort or key deterministic state output by pointer address.
- For AST-specific work, read `common/ast/AGENTS.md`.

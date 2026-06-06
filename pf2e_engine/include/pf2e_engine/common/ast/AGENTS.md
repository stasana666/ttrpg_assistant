# AST State Comparison

## Purpose
- Deterministic AST serialization used to compare battle state before and after save/rollback.

## Key Files
- `ast_node.*`: move-only tree nodes, structural equality, pretty-print, and diffs.
- `ast_context.*`: identity registration, pending pointer resolution, and ownership-cycle detection.
- `ast_helpers.h`: helpers for value fields, owned objects/containers, references, and callback placeholders.
- `ast_layout_assert.h`: layout checks for handwritten AST-instrumented classes.

## Conventions
- Use `AddValueField` for non-recursive values, `AddOwnedObject`/`AddOwnedContainer` for owned recursive state, and `AddReference` for non-owning pointers.
- Register stable identities for owned subobjects before references need to resolve.
- Keep AST output deterministic; sort unordered containers by stable ids, enum values, registered names, or vector indices.
- Handwritten AST-instrumented classes should keep `ast_layout_sentinel_` last and update layout asserts after auditing `GetAst`.

## Pitfalls
- Generated schema classes do not need handwritten layout asserts; the schema drives generated `GetAst`.
- Unresolved pending references show up as empty placeholders in diffs.
- `std::function` fields can only be placeholder-compared by presence and target type.

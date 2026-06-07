# AST State Comparison

Canonical documentation file. AGENTS.md must remain semantically equivalent.

## Purpose
- Deterministic AST serialization used to validate save/rollback correctness.
- Tests compare battle state before and after rollback to catch mutations that bypass `TTransformator`.

## Key Files
- `ast_node.h/.cpp`: `TAstNode` tree, structural equality, pretty-print, diff paths. Children use `std::list` so child pointers survive parent moves.
- `ast_context.h/.cpp`: identity table, pending-pointer resolution, ownership-cycle detection.
- `ast_constructable.h`: `TIsAstRecursive<T>` trait.
- `ast_serialize.h`: value serialization overloads.
- `ast_helpers.h`: helpers for values, owned objects/containers, references, and callback placeholders.
- `ast_layout_assert.h`: layout assertion macros for handwritten AST-instrumented classes.

## Helper Rules
- Use `AddValueField` for non-recursive values such as ints, enums, strings, and positions.
- Use `AddOwnedObject` / `AddOwnedContainer` for owned AST-recursive state.
- Use `AddReference` / `AddReferenceContainer` for non-owning pointers.
- Use `AddCallbackPlaceholder` for `std::function`; it compares presence and target type only.
- Register stable identities for owned subobjects with `ctx.RegisterIdentity` before or during traversal. References may resolve later through the pending-pointer mechanism.

## Determinism
- AST output must be byte-identical for equal state across runs.
- Sort unordered containers before emission by stable keys: enum values, `TValueIdManager::Name(id)`, `TPlayer::GetId()`, or vector indices.
- Do not sort by pointer address or by a reference string that might not be resolved yet.
- Filter logically zero entries from condition/resource maps when zero is equivalent to absence.
- `TVariantMap` uses `std::map`, so generated variant sets iterate deterministically.

## Layout Safety
- Handwritten AST-instrumented classes should keep `ast_layout_sentinel_` as the last private member when possible.
- `GetAst` should open with `AST_ASSERT_LAYOUT_WITH_SENTINEL` for standard-layout classes or `AST_ASSERT_LAYOUT` when sentinel offsets are not standard-safe.
- If a layout assert fails after adding a field, audit `GetAst`, add the field with the correct helper, then update expected size/offset constants.
- Generated schema classes do not need handwritten layout asserts; schema-driven generation owns their `GetAst`.

## Pending-pointer resolution
- A transformation record holds a raw pointer (e.g. `THitPoints*`) with no idea who owns it, yet the AST must render it as `"ref:player#3.creature.hitpoints"` — and the owning `GetAst` may run *after* the reference's. No separate finalization pass.
- `AddReference(node, label, ptr, ctx)`: if `IdentityOf(ptr)` is known, emit `"ref:<id>"` eagerly; otherwise add an empty Value node and `RegisterPending(ptr, &node)`. `RegisterIdentity(p, id)` records the name *and* back-fills any nodes already queued for `p`. Registration order therefore does not matter.
- This relies on child-node pointers staying valid until the end of the enclosing `GetAst` — which is exactly why children live in `std::list` (move/splice keep element pointers stable; `std::vector` would invalidate them on realloc).
- A pointer that is never registered (dangling, or its class was not traversed) leaves the placeholder empty, showing up as a content mismatch in a diff.

## Cycle detection
- `TAstContext::Visit`/`Unvisit` (via `TGuard`, called from `AddOwnedObject`/recursion) throw on a duplicate insertion.
- Keyed on the **(address, type) pair** (`std::pair<const void*, std::string>`), not address alone: C++ layout often places a class and its first non-empty member at the same byte offset (e.g. `TCharacteristicSet` begins with `stats_[0]`), so an address-only set would falsely flag a cycle.
- Non-owning references never call `Visit`, so they cannot create traversal cycles.

## Adding a new AST-instrumented class
1. Header: include `ast_constructable.h`; add `TAstNode GetAst(TAstContext&) const`; add `[[maybe_unused]] char ast_layout_sentinel_[1] = {};` as the **last** private member; add `template <> struct TIsAstRecursive<TFoo> : std::true_type {};`.
2. Cpp: include `ast_helpers.h` + `ast_layout_assert.h`; open `GetAst` with `AST_ASSERT_LAYOUT[_WITH_SENTINEL]`; use the right `Add*` helper per field; if you own a stable name for a subobject, `ctx.RegisterIdentity` it near the top.
3. The first build fails with the actual `sizeof`/sentinel offset in the error — copy those into the `kExpectedSize` / `kExpectedSentinelOffset` constants.

## Tests
- `pf2e_engine/tests/ast/test_ast_state.cpp`: identical battles, mutation visibility, save/mutate/rollback equality, direct-mutation bypass detection, determinism, null handling, and cycle detection.
- `ast_test_fixture.h`: `MakeTwoWarriorBattle()` fixture.

## Known Limitations
- `TPlayer::SetPosition` mutates position and battle-map cells without `TTransformator`; `BypassDetected_SetPosition` documents this.
- Non-standard-layout classes get sizeof-only checks.
- `std::function` fields are placeholder-compared and cannot be structurally compared.

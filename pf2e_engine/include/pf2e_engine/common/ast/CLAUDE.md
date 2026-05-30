# AST-based state comparison

Used to validate save/rollback correctness. Each engine class exposes a
`TAstNode GetAst(TAstContext&) const`; tests assert AST equality before vs.
after rollback to catch any state mutation that bypassed `TTransformator`.

## Files

- `ast_node.h/.cpp` — `TAstNode`: Null / Value / Object kinds; structural
  equality; pretty-print; diff-with-path. Move-only. Children stored in
  `std::list` so pointers to child nodes survive moves of the parent.
- `ast_context.h/.cpp` — `TAstContext`: identity table, pending-pointer
  resolution, ownership-cycle detection.
- `ast_constructable.h` — `TIsAstRecursive<T>` trait.
- `ast_serialize.h` — `AstSerialize(...)` overloads for value-typed fields.
- `ast_helpers.h` — `AddValueField` / `AddOwnedObject` / `AddOwnedContainer`
  / `AddReference` / `AddReferenceContainer` / `AddCallbackPlaceholder`.
- `ast_layout_assert.h` — `AST_ASSERT_LAYOUT` and
  `AST_ASSERT_LAYOUT_WITH_SENTINEL` macros.

## Helpers — when to use which

`AddValueField(node, label, v)` for plain value-typed fields (int, enum,
string, TPosition, etc.). The `!TIsAstRecursive<V>` static_assert rejects
accidental pointer or AST-recursive types.

`AddOwnedObject(node, label, obj_or_ptr, ctx)` for owned children — recurses
into `obj.GetAst(ctx)`. Static_assert requires `TIsAstRecursive<T>` to be
true. Null pointers emit a Null node.

`AddOwnedContainer(node, label, c, ctx, key_fn)` for containers of owned
elements (vector/list/map/unordered_map of value or smart-pointer/raw-pointer
to AST-recursive). `key_fn(elem, idx) -> sortable key` makes the output
deterministic regardless of the container's iteration order.

`AddReference(node, label, ptr, ctx)` for non-owning pointers. Emits
`"ref:<stable_id>"` once the target's id is registered via
`ctx.RegisterIdentity(ptr, id)`. The registration may happen later, including
after this call — see "Pending-pointer resolution" below.

`AddReference(node, label, ptr, stable_id_sv)` is the eager overload for
well-known fixed names (e.g. `"battle.map_holder"`).

`AddReferenceContainer(node, label, c, ctx, ptr_fn)` is the container form.

`AddCallbackPlaceholder(node, label, fn)` for `std::function` fields. Emits
presence + `target_type` only; behavioural equivalence cannot be compared.
Safe only when `fn` is restored verbatim across save/rollback (see "Known
limitations").

## Pending-pointer resolution

The hard problem: a transformation record holds a raw `THitPoints*` and has
no idea who owns it. We want the AST to render that pointer as
`"ref:player#3.creature.hitpoints"`, but the owning `TPlayer::GetAst` may run
*after* the transformation's `GetAst`. We don't want a separate
finalization pass.

Mechanism in `TAstContext`:

- `RegisterIdentity(p, id)` records the stable name AND walks any nodes
  previously queued for `p` (via `RegisterPending`), filling them in.
- `AddReference(node, label, ptr, ctx)` — if `IdentityOf(ptr)` is known,
  emit `"ref:<id>"` eagerly; otherwise add an empty Value node and call
  `RegisterPending(ptr, &just_added_node)`.

This relies on the node pointer staying valid until the end of the current
`TBattle::GetAst` call. We use `std::list` for children precisely because
`std::list::splice` / move semantics keep element pointers stable across
parent moves; `std::vector` would invalidate them on every realloc.

`TBattle::GetAst` registers only the names it owns (subsystem and per-player
top-level names). Each class registers its own sub-objects inside its own
`GetAst` (e.g. `TPlayer` registers its `creature_` as `"player#X.creature"`;
`TCreature` registers `hitpoints_` and `resources_` as
`"player#X.creature.hitpoints"` etc.). Order doesn't matter.

If a pointer is never registered (dangling, or class wasn't traversed), the
empty placeholder stays empty — visible as `"content mismatch (\"\" vs ...)"`
in a diff against any AST that did resolve it.

## Cycle detection

`TAstContext::Visit(p, type) / Unvisit(p, type)`, wrapped in
`TAstContext::TGuard`, are called from inside `AddOwnedObject` /
`RecurseElement`. Throws on a duplicate `(p, type)` insertion.

Keyed on the **(address, type) pair**, not address alone. C++ layout often
puts a class and its first non-empty member at the same byte offset
(`TCharacteristicSet` begins with `std::array<TCharacteristic, 6> stats_`
whose `stats_[0]` is at offset 0). A pure-address set would falsely flag
that as a cycle every traversal.

Non-owning references never call `Visit` and therefore cannot create
traversal cycles.

## Layout safety

Every AST-instrumented class adds:

```cpp
[[maybe_unused]] char ast_layout_sentinel_[1] = {};
```

as its last private member, and its `GetAst` opens with:

```cpp
AST_ASSERT_LAYOUT_WITH_SENTINEL(Class, kExpectedSize, kExpectedSentinelOffset);
```

The pair (sizeof + sentinel offset) catches any new field — whether it
changes total size or exactly fills trailing alignment padding.

Non-standard-layout classes (those with reference members, non-empty bases,
or private nested types that bleed into the parent) cannot use `offsetof`
under the C++ standard. Those classes use plain `AST_ASSERT_LAYOUT` (sizeof
only); a field that perfectly fills trailing padding goes uncaught. Affected
classes today: `TBattle`, `TInitiativeOrder`, `TTransformator`, `TCreature`,
`TPlayer`, `TEffectManager`, `TProficiency`, `TTaskScheduler`, `TWeapon`,
`TCharacteristic`, `TCharacteristicSet`, `TCreatureFeat`.

When you add a field to an instrumented class, the build will fail with a
sizeof or sentinel-offset mismatch. Re-audit `GetAst` (add the new field via
the right helper), update the expected constants, done.

## Determinism

The AST must be byte-identical across runs and identical for two
independently constructed equal states.

- Unordered containers (`unordered_map`, `unordered_set`, `multiset` keyed on
  pointers) MUST be pre-sorted by a build-time-stable key before emission.
  Stable keys are: enums, `TValueIdManager::Name(id)`, `TPlayer::GetId()`,
  vector indices. Pointer addresses and resolved string refs are NOT stable
  build-time keys (the former varies across runs, the latter may not be
  resolved yet at sort time).
- `TConditions` and `TResourcePool` filter out 0-valued entries —
  `TChangeCondition::Undo` / `TChangeResource::Undo` restore the value but do
  not erase the map key, leaving stale `{key, 0}` entries that would
  otherwise fail rollback equality. `TCreature::Get(condition)` returns 0
  for both absent and zero-mapped keys, so this matches logical state.
- `std::function` fields cannot be compared structurally; the placeholder
  emits presence + target_type only. AST equality across rollback holds
  because `TRemoveTask::Undo` and `TRemoveEffect::Undo` reinstall the same
  object verbatim.

## Tests

`pf2e_engine/tests/ast/test_ast_state.cpp` covers: equality of identical
battles; per-mutation diff visibility; full save→mutate→rollback equality;
direct-mutation bypass detection (the existing `TPlayer::SetPosition` bypass
is documented here); container determinism; null handling; cycle detection.

`ast_test_fixture.h` provides `MakeTwoWarriorBattle()` — factory + mocks +
two warriors on a simple map.

## Known limitations

- `TPlayer::SetPosition` mutates `position_` and battle-map cells WITHOUT
  going through `TTransformator`. Test `BypassDetected_SetPosition`
  documents this. Follow-up: add `TChangePosition` transformation.
- Non-standard-layout classes (see "Layout safety") get sizeof-only checks.
- `std::function` fields are placeholder-compared (see "Determinism").

## Adding a new AST-instrumented class

1. In the header: include `ast_constructable.h`, add
   `TAstNode GetAst(TAstContext&) const`, add
   `[[maybe_unused]] char ast_layout_sentinel_[1] = {};` as the last private
   member, and add `template <> struct TIsAstRecursive<TFoo> : std::true_type {};`.
2. In the cpp: include `ast_helpers.h` and `ast_layout_assert.h`; implement
   `GetAst` opening with `AST_ASSERT_LAYOUT(_WITH_SENTINEL)`. Use
   `AddValueField` / `AddOwnedObject` / `AddOwnedContainer` /
   `AddReference` per the rules above. If you own a stable name for any
   sub-object, register it with `ctx.RegisterIdentity` near the top.
3. First build attempt will fail with the actual `sizeof(Class)` and
   `offsetof(ast_layout_sentinel_)` in the error message — fill them into
   the `kExpectedSize` and `kExpectedSentinelOffset` constants.

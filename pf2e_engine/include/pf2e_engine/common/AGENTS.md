# pf2e_engine common utilities

Canonical project instructions. CLAUDE.md redirects here.

Shared low-level building blocks used across the engine. These utilities carry
non-obvious design constraints worth knowing before changing them.

## Ability catalogs and source traversal (`ability.h`)

`TAbilityCatalog` owns definitions keyed by module-qualified IDs. Identical repeated
registration is harmless; conflicting definitions, invalid kinds, and grants on
non-feature definitions are rejected. Publish catalogs through const ownership
after loading. Executable pipelines live separately in `TAbilityExecutions` under
`actions/ability.h`; generated schema headers depend only on the common catalog.

Generated `VisitGrantedAbilities` methods traverse current source fields and
collection entry IDs into a fresh `TAbilityCollector`. They do not maintain a
second action list. The collector recursively expands features, rejects cycles and
unknown references, and deduplicates by definition ID plus typed source path while
retaining distinct provenance chains. A failed `Grant` expansion or nested `Visit`
does not publish partial grants. `TFeatureSelection` snapshots store a definition
ID string or null for an empty default selection; a nonempty selection must
resolve to a feature.

`CollectAbilities(value, catalog)` traverses any generated provider without adding
rules ownership to the generated value. `Grants()` contains action and reaction
definitions; `Features()` contains every selected or recursively expanded feature,
including features without terminal grants. Both collections preserve source and
provenance and publish atomically after successful traversal.

Paths compare field and entry segments rather than display strings. Removing a
source affects the next traversal immediately and leaves other source instances
intact. Gameplay collection mutation still requires the stable journal resolver
described below; construction-time removal alone does not add rollback support.

The action adapter produces value options with actor, source, definition, and
optional reaction-trigger bindings. Enumeration uses const state and no interaction,
RNG, resource writes, or ID allocation. Requirement callbacks must remain pure.
Missing implementations and mismatched action/reaction bindings are configuration
errors. Execution recollects sources and checks requirements and aggregated costs
again before consuming resources. Reaction enumeration does not execute reactions.

Options temporarily anchor actor instances with non-owning player addresses in
addition to display IDs. Execution compares these addresses without dereferencing
them, rejecting options from another battle with reused player IDs. Players must
remain at stable addresses during a battle; these anchors are not persistent entity
IDs and must not be serialized or used after that battle's lifetime.

`TActionContext` retains the immutable pipeline and an owned source-weapon snapshot.
The battle adapter resolves source paths and supplies a fresh-grant validator.
Source validation runs before each block, after feature hooks, and before every
resumed continuation, including a second suspension inside an earlier resume.
An unavailable source raises `TAbilityUnavailable`; previously spent action costs
are not charged again or automatically refunded. This continuation-aware execution
does not wrap a yielding action in the synchronous generated-method transaction.

`TBattle` retains one immutable `TAbilityRules` snapshot supplied by the factory.
Its `FeatureHooks` map owns legacy pipeline definitions outside creature state.
Block hooks are selected from the current `Features()` traversal and deduplicated
per definition and source. Each suspended hook retains its definition and a scoped
source validator. Every resume reinstalls that validator before its inner callback,
including replay of a previously completed savepoint after rollback. Completion,
suspension, and failure restore the previous validation scope. Removing a hook
source aborts that continuation with `TAbilityUnavailable`; the caller discards the
failed action context or restores the source before replaying the savepoint.

Finalize factory definitions before taking its rules snapshot. Existing battles
retain their original snapshot if the factory later changes. Creature snapshots
store feature IDs, not hook or action implementations, and must be restored with
the corresponding rules catalog. Entities, players, and the battle must remain
alive and at stable addresses while bound, journaled, or referenced by savepoints.
Battle insertion rejects missing, moved-from, and duplicate creature identities
and duplicate player IDs before binding map cells. A failed initiative insertion
restores the map and player list.

## Compiled action execution (`actions/generated_action.h`)

Generated actions use `TAction::FBody` and direct C++ continuations. They do not
build action-reader pipelines or invoke legacy block hooks. Resource costs bind
generated `Resource` field paths through separate const reads and guarded writes;
payment aggregates aliases, validates all bindings and availability first, then
journals payment atomically. Costs are paid once before the body starts.

Entity bindings retain type, identity and a stable participant address. Reads,
guard projection and choice resumes validate the binding; actor execution also
checks the current battle eligibility policy. The native battle adapter currently
provides `TCreature` participants. The compiler has no hard-coded creature type,
HP field names or resource categories. Future native entity kinds need adapters.

`IInteractionSystem::RequestChoice` either invokes its receiver synchronously or
suspends through `TSavepointStackUnwind` with an owned callback. It must not return
without delivering a choice or suspending. Generated callbacks capture earlier
locals by value and create fresh locals on every invocation so an old savepoint
can be replayed after rollback. The selected entity and its predicate are checked
again on resume; the enclosing execution also rechecks the granting source.

A yielding action is not enclosed in a synchronous method transaction. A failed
body leaves its paid costs and completed mutations in the caller's journal until
that caller undoes them. Empty choices and failed requirements follow this same
policy. Whole-action cancellation and target preflight remain separate work.

## Typed action sources (`common/ability_source.h`, `actions/generated_source.h`)

Generated `VisitAbilitySources` exposes stored class providers under the same
field/collection paths used by grants. `FindAbilitySource<T>` selects the nearest
ancestor of a grant's source path whose type matches `T`. Discovery reads the
schema-generated structure; it does not depend on weapon field names or a native
source-kind enum. The located pointer is only a construction-time borrow.

An action declared `using TWeapon Weapon` checks its required source during
availability, before resource payment. Its generated immutable execution also
prepares an owned readonly snapshot before payment. Reads and resumes validate
the actor, current grant and provider path while retaining the snapshot's values.
Replacing a provider at the same stable path can preserve eligibility; removing
the grant or provider invalidates the continuation. Existing snapshots do not
silently change to the replacement's properties.

`TAbilityExecutions` accepts shared immutable execution definitions, allowing a
generated action's requirement/preparation callbacks to register idempotently by
object identity. Independently supplied conflicting callbacks are still rejected.
`ExecuteAbility` resets prepared source state when reusing a context, then prepares
the new binding before `TAction::Apply`. Preparation must not mutate the world.
Snapshot validators capture the owner, rules catalog and previous validator rather
than their owning context, avoiding an ownership cycle. Their battle and entities
must remain alive for the binding's lifetime.

## Typed feature events (`common/feature_event.h`, `actions/feature_event.h`)

`TAbilityRules::Events` stores immutable handlers by event identity and feature
definition. Generated event tags retain their C++ owner, argument and result types;
the registry rejects conflicting tag types or handlers and permits registration
of the same shared handler more than once. Feature hooks do not require terminal
action grants: dispatch reads `CollectAbilities(owner, Catalog).Features()`.
The common registry has no dependency on creature or action types. Generated
registrars retain static shared handlers so repeated imports preserve identity.

Each feature definition and source contributes once, while the same definition
selected through different sources contributes separately. Results retain the
value, feature ID, source path, provenance and owner ID in traversal order.
Dispatch constructs a fresh result sequence and publishes it only after all handlers succeed. The
sequence is a snapshot; later source changes affect the next dispatch, not a
value already retained across an action's suspended choice.

Handlers receive a readonly world view, readonly entity bindings and copied value arguments. They are
synchronous and pure; nested dispatch and suspension are rejected. The context
overload validates the acting participant and membership of owner/entity arguments
in that battle. The rules-only overload permits readonly evaluation outside an
action and does not impose actor eligibility on the event owner. Its caller must
keep the referenced entities and rules snapshot alive.
The explicit world overload also validates the owner and entity arguments against
that world; the rules-only overload supplies an unbound view. Pure handlers that
do not use world operations can still run without a battle.
Readonly capabilities preserve identity while rejecting `Guard()` and mutable
participant access. The recursion guard is thread-local. Native handlers must
obey the same purity contract as generated handlers; captured external references
cannot be sandboxed by the registry.

## Rule world queries (`actions/generated_world.h`)

`TGeneratedWorldView` borrows a battle and exposes readonly participant bindings,
positions, teams and line-of-effect queries. Every operand is checked for current
identity and membership in that battle; a participant ID alone is not sufficient.
`Entities<T>()` returns a snapshot of readonly bindings, including dead participants.
Rules decide which participants are eligible, and properties are read from the
current objects. Battle and participants must outlive the view and continuations
that capture it. Unbound views permit ordinary readonly binding but reject spatial
queries and participant enumeration.

The language operations are `entities(Type)`, `grid_distance(A, B)`,
`same_team(A, B)`, `has_line_of_effect(A, B)`, `position_x(A)` and `position_y(A)`.
Grid distance means Chebyshev distance in map cells; PF2e diagonal movement costs
are not part of this generic metric. The current map has no obstacle model and
its line-of-effect check always succeeds.

An action's `requires` declarations are pure preconditions checked during
enumeration and immediately before payment. A first `choose` immediately after
the `cost`/`requires` prefix also supplies a target-existence precondition. Later
choices are not evaluated ahead of intervening computations or effects. The
existing imperative `require` remains an assertion at its position in the body.
Preconditions read current source values before payment; execution uses its
prepared source snapshot. Preconditions are not perpetual invariants after an
action starts. A chosen target's `where` predicate is checked again on receipt,
using current target properties and the retained source snapshot.

## Generated method execution (`generated_execution.h`)

`TGeneratedExecutionContext` is a lightweight handle onto the existing
`TTransformator` journal. Generated methods receive it as their hidden first C++
argument. They open `auto transaction = ctx.Atomic()`, perform writes through
`ctx.Assign(field, value, this, "field_name")`, and call `transaction.Commit()`
only after successful completion. A failed or uncommitted scope undoes all changes
since its checkpoint, including committed nested calls and legacy transformations.
Nested methods and C++ adapters call `ctx.Invoke(guarded, method, args...)` to access
a `TGuarded<T>` without exposing a mutable getter to other code.
For fixed nested fields, `ctx.Project(owner_guard, getter)` produces another
guarded reference, and `ctx.AssignGuarded(field_guard, value, owner_guard, name)`
records the same controlled assignment. Projection requires a guarded result and
does not expose the underlying mutable reference to callers.

Assignments stage the replacement and append a journal record before a noexcept
swap changes the field. Value types must be copy constructible and nothrow
swappable. Journal AST includes the owning object's identity, field name, and old
value. Owners must register their stable identity during AST traversal.
Native callers can journal a complete `TIdCollection` replacement when the
collection itself stays at a stable field address. Its previous-value AST records
entry IDs, values and the next ID; Undo restores the allocation history as well as
the visible items. This does not permit replacing a collection while retaining
journal records that point into its entries.

Field addresses and owners must remain valid and unmoved while their history is
retained. Do not use these methods on movable collection elements, or replace an
aggregate after journaling writes into its children. Rule-language entity and
container mutation will require stable owner/field-path resolution before it can
be supported generally.
Generated transactions currently support synchronous methods only: suspension via
`TSavepointStackUnwind` is not a method failure and needs separate continuation-aware
transaction handling before methods may prompt, yield, or invoke reactions.
Checkpoints are bound to their originating transformator; undo rejects a foreign
checkpoint or a depth no longer present in the current journal.

## Collection snapshots (`id_collection.h`)

`TIdCollection<T>` allocates monotonically increasing typed IDs. Removing an entry
does not return its ID to the allocator. Snapshots must store both each explicit
entry ID and `NextId()`; serializing values alone loses holes and allocation history.
Restore into a fresh temporary collection with `RestoreEntry(id, value)`, then
`RestoreNextId(next)` before publishing the result. Restore rejects duplicate IDs
and a next ID at or below any stored entry. JSON readers must reject negative and
non-integral IDs before converting them to `size_t`.

The maximum `size_t` value is reserved for an exhausted allocator and cannot be an
entry ID. `Add` throws on exhaustion instead of wrapping or reusing IDs. These
restore operations construct snapshot state and do not perform journaled gameplay
mutation.

## Continuation helpers (`continuation.h`)

Continuation-aware execution helpers. Gameplay code occasionally needs to
*suspend*: a step throws `TSavepointStackUnwind`, the stack unwinds, and the work
can be `Resume()`d later from exactly where it stopped. For that to work, every
frame between the suspension point and the top-level handler must record how to
finish the rest of its own work once the savepoint resumes. These helpers
centralize that bookkeeping — they know nothing about battles, creatures, or
actions, only about propagating continuations correctly across the call stack.
They are exception-correct and support arbitrary depth.

- `Then(step, tail)` — runs `step`; if it suspends, `tail` is scheduled to run
  when the savepoint resumes, otherwise `tail` runs now. **`Then` is intentionally
  recursive:** the scheduled continuation re-enters `Then` rather than plainly
  running `resume(); tail();`. That way, if `resume()` itself suspends again,
  `tail` is re-protected onto the new savepoint instead of being silently dropped.
- `While(condition, body)` — repeats `body` while `condition` holds; if `body`
  suspends, the loop resumes with the remaining iterations.
- `ForEach(begin, end, func)` — iterates the range `[begin, end)`. **The range must
  outlive any suspension** — use it only for stable storage such as a member
  container. For a temporary container use `ForEachOwned` instead.
- `ForEachOwned(container, func)` — takes ownership of `container`. The elements
  are moved onto the heap (`shared_ptr`) and that handle is captured into every
  continuation level, so they stay alive across stack unwinding (unlike a plain
  iterator range over a local container, whose storage would dangle).

## `TVariantMap` (`variant_map.h`)

Container for a closed sum type (a generated `variant`) keyed by its discriminant.
A weapon never carries two traits of the same kind, and rules code constantly asks
"does this object have trait X, and with what parameter?" — so `set<Variant>`
lowers to this rather than `std::set<Variant>`.

- `TKind` is the variant's kind enum (e.g. `EWeaponTraitKind`); `TVariant` is the
  generated wrapper class (e.g. `TWeaponTrait`), which must expose `TKind Kind() const`
  and `template <class T> const T* TryGet() const`. Each alternative payload struct
  `T` must expose `static constexpr TKind Kind`.
- `Get<T>()` is a typed lookup: returns `nullptr` if absent or a different alternative.
- Backed by `std::map`, which iterates in sorted key order, so iteration is
  **deterministic** — the AST state-comparison relies on this.

`overloaded<Ts...>` is the standard overload-set builder for exhaustive `std::visit`
over a variant's payload. Adding an alternative to the DSL and regenerating makes
any visit site built with this fail to compile until the new case is handled.

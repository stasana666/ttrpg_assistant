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
Legacy wrappers and block entry also check the actor's current `CanAct()` state;
an action suspended before unconsciousness or death cannot resume its effects.
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

Generated actions use explicit `TAction::TCompiledBody` and direct C++ continuations. They do not
build action-reader pipelines or invoke legacy block hooks. Resource costs bind
generated `Resource` field paths through separate const reads and guarded writes;
payment aggregates aliases, validates all bindings and availability first, then
journals payment atomically. Costs are paid once before the body starts.

Entity bindings retain type, identity and a stable participant address. Reads,
guard projection and choice resumes validate the binding; actor execution also
checks the current battle eligibility policy. The native battle adapter currently
provides `TCreature` participants. The compiler has no hard-coded creature type,
HP field names or resource categories. Future native entity kinds need adapters.

`ValidateActionActorBinding` checks a captured actor against the current context
using both entity identity and participant address. Generated callable frames
retain that original binding across suspension instead of resolving a new owner
on each resume. The check preserves source and actor eligibility validation;
when a battle is bound, the current participant must belong to it. Readonly and
mutable aliases compare as the same actor without changing their capabilities.

`IInteractionSystem::RequestChoice` either invokes its receiver synchronously or
suspends through `TSavepointStackUnwind` with an owned callback. It must not return
without delivering a choice or suspending. Generated actions execute a compiled
block graph with a typed frame. Ordinary block transitions are iterative; each
suspended choice captures a frame value and creates a fresh copy on every resume.
Lexical slots are cleared when their scope ends. Branches and `while` loops retain
changes to outer locals without sharing mutable continuation state.

`SelectActionValue` and `SelectActionEntity` return immediate choices directly;
their continuation runs only when a saved prompt resumes. This prevents a long
sequence of immediate choices from growing the C++ call stack. A completed or
failed prompt cannot be invoked later as a continuation. A suspended prompt can
be replayed, and validates its original actor, candidate identity, predicate and
granting source each time. An explicit entity choice may select from readonly
world bindings and obtains a new mutable binding; existing aliases stay readonly.

A yielding action is not enclosed in a synchronous method transaction. A failed
body leaves its paid costs and completed mutations in the caller's journal until
that caller undoes them. Empty choices and failed requirements follow this same
policy. A leading choice has target-existence preflight; choices after effects or
inside control flow do not. Whole-action cancellation remains separate work.

## Generated movement (`actions/generated_movement.h`)

`Position` action values lower to `TPosition`. World adjacency returns in-bounds
eight-neighbor values in row-major order, excluding the origin; it does not retain
cell pointers. `Contains` checks map bounds, and `CanOccupy` validates the participant
and permits its current cell or an empty destination.

`MoveActionEntity` validates actor/source, participant membership, mutable access,
the expected origin, a single adjacent step, bounds and occupancy before journaling
movement. It has no implicit resource cost, PF2e distance formula or reaction window.
An explicit origin retained across a prompt rejects a step after forced movement.
Legacy `MovePlayer` remains usable for nonadjacent relocation. Map cell access and
participant binding check bounds, and journal capacity is secured before the move
constructor changes state. Existing journal entries relocate without throwing.

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

## Selected action sources (`actions/generated_source.h`)

`SourceCandidates` enumerates stable collection entry IDs through a generated
const getter. Each candidate retains an owner binding and exact getter/entry
locator, never a borrowed collection element. A missing entry cannot resolve to
an ancestor or another provider. The getter must return a reference to a stored
collection, and its generated path must describe that collection. No ability
grant on the selected value is required. Union enumeration preserves the first
candidate for each owner/type/path key.

Candidate predicates read current values at offer and receive. `SelectActionSource`
creates the immutable value snapshot only after selection passes its current
predicate; subsequent attribute changes affect future selections. Its result
still validates owner identity, current-world membership and exact entry presence.
The continuation runs only for suspended choices, and each replay creates its own
selected snapshot. Labels are supplied by generated display getters and duplicate
labels are disambiguated without using them as source identities.

`ForkActionInputs` copies the parent's execution context and adds an immutable
list of input validators while sharing its actor, services and journal. Validators
capture bindings, never the child context. They use the leaf
`ValidateActionEntityMembership` plus exact lookup; calling full actor/source
validation from an input validator would recursively enter the same chain.
`ValidateAbilitySource` checks both the granting source and these inputs. The
child callable uses the fork, while its return channel resumes the parent context,
so input constraints do not outlive their lexical caller scope. Starting a new
top-level ability clears any earlier input constraints on a reused context.

## Typed reaction windows (`actions/reaction_execution.h`, `reaction_window.h`)

`TAbilityRules::Reactions` contains immutable typed executors keyed by trigger and
reaction definition. A definition can support several trigger types. Registration
permits the same shared executor again and rejects conflicting executors or trigger
identities. `RequireGrantImplementation` routes terminal grants to exactly one
matching action, legacy reaction or typed reaction backend. Legacy `ETrigger`
enumeration intentionally skips validated typed reactions through its rules-aware
overload; the two protocols remain separate.

Typed `.ttrpg` triggers and reaction bodies compile to these windows.
`TReactionExecution<Tag>` carries an owner type, pure
requirement, optional source preparation, bound resource payment and a yielding
body. The body's state and argument types come from the trigger tag. Entity
arguments keep their supplied capabilities; requirements receive readonly aliases.
Persistent changes use the shared transformation journal. Resource formulas,
disruption, pending damage reduction and event timing belong to authored rules.

`OpenReactionWindow` visits participants in insertion order, once each per window.
It recollects current grants for each participant and explicitly offers decline
alongside eligible reactions. Selection rechecks identity, grant/source, requirement
and resources before preparation and atomic payment. `TResourcePayment` is also
used by `TAction`, preserving alias aggregation and read/write identity checks.
A nonempty `LimitKey` records the reactor in a copied `TReactionScope` after payment;
passing the returned scope through several windows restricts the same reaction
across different source grants during one composed activity. Generated `once per activity`
triggers use their canonical trigger ID as the key, sharing the limit across all
subscribing reaction definitions for each reactor. Each invoked activity gets a fresh
scope; windows inside its loops reuse the returned scope. Declining records no
use. Ordering and one offer per participant are the current native protocol, not a
complete PF2e simultaneous-trigger policy.

Window frames copy the pending value, arguments, scope and participant cursor.
Immediate calls return without invoking their continuation; suspended calls return
through that continuation, including repeated suspension and replay. A small
shared protocol flag rejects immediate and late body callbacks; it stores no result
or mutable continuation frame. Bodies must follow the same validation contract as
compiled activities, including captured actor/input validation at resumed edges.
All completed effects and prices remain journaled after a failed body until the
caller explicitly undoes them.

Offer contexts and execution contexts are separate objects. Both inherit only a
snapshot of battle, journal and services. An execution owns its reactor binding,
registry, immutable executable, granting source and trigger argument validators.
It does not inherit parent grant or selected-input validators. Parent continuation
validation happens after the reaction/window finishes. `TCompiledBody` consequently
runs without the legacy outer `RunValidatedAbility` savepoint wrapper; ordinary
native `FBody` and pipelines retain that wrapper. The battle drains an existing
savepoint even after the parent dies or the battle reaches its end condition.
Once that continuation finishes, the battle does not start another participant's
turn after its end condition has been reached.

Nested windows retain an immutable trigger trace in their contexts. Different
triggers may nest; an already active trigger is rejected to prevent native cycles.
No trace or scope requires global mutable state or restoration after a suspension.
Battles and participants must remain alive and at stable addresses for all retained
contexts and continuations. Pure callbacks must not mutate, roll dice or suspend.

## Generated lifecycle execution (`actions/lifecycle_execution.h`)

`TAbilityRules::Lifecycle` binds round start, turn start and turn end to immutable
typed-owner activity executors. Battle boundaries dispatch these hooks through a
fresh service context; rules without a hook retain their previous generated-method
fallback. The factory currently binds `StartCreatureTurn` to turn start. PF2e recovery,
feature contributions, dice and resource resets remain in `.ttrpg`.

Lifecycle contexts use internal `System` execution mode so a dead or unconscious
actor can undergo automatic rule transitions. Identity, current battle membership,
captured actor and input validation still apply. Ordinary action entry explicitly
resets that mode and drops the lifecycle owner; reactions construct separate ordinary
contexts. Being unable to act never grants an ordinary action through a lifecycle.

`InvokeLifecycle` returns immediately without invoking its continuation when the
body completes synchronously. Only a suspended body may call that continuation;
completed and failed bodies cannot invoke a retained callback later. Activity frames
retain all remaining work and copy local values on every resume. `RunLifecycleAndDrain`
reverts and drains successive savepoints with the original journal without restarting
the phase or rerolling earlier dice. It does not make synchronous scheduler callbacks
yield-aware. Participants, battle and journal must outlive retained continuations.

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

Stored variant sets support journaled `SetVariant` and `EraseVariant` operations.
They prepare a replacement before changing the fixed container field. Variant
payload reads remain copies, so these operations do not expose mutable entries.
`AddResource`, `SpendResource` and `SetResourceCount` similarly assign a checked
replacement. Counts must be nonnegative; spending requires enough resources and
addition checks integer overflow. Generated rule syntax exposes `.set`/`.erase`
and `Resource.Add`/`Spend`/`SetCount`, with implicit context and guarded ownership.

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

## Initiative relocation

`actions/generated_initiative.h` exposes journaled movement before the current
turn's participant. It validates the action actor, live source, target identity,
battle membership and mutable capability before any operation, including the
setup no-op. A reaction's executing actor is not the anchor. Initiative relocation
does not inspect hit points or apply game-specific knockout policy. The order uses
stable list iterators; each journal record retains the target's previous successor
and restores it with LIFO Undo. Participants and the order must outlive that history.

## Generated construction

`construction.h` contains constructor reports and feature-choice descriptors shared
by generated factories and native interfaces. Pending dependencies are distinct
from disabled choices. `generated_construction.h` is a friend of generated classes
and creates private temporary storage without evaluating rule defaults. This is
an implementation detail used by construction and restore, not a gameplay mutation
or an author-facing declaration form. A successful generated entity construction
allocates identity only after all stored field initialization succeeds.

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

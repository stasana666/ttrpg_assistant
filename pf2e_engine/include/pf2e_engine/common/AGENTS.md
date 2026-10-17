# pf2e_engine common utilities

Canonical project instructions. CLAUDE.md redirects here.

Shared runtime building blocks for generated rules. Each section lists the invariants that are not obvious from the code.

## Abilities and execution (`ability.h`, `actions/ability.h`)
- `TAbilityCatalog` owns definitions keyed by `<module>.<Name>`. Identical re-registration is harmless and conflicts throw. Executable bindings live in `TAbilityExecutions`, `TReactionRegistry`, `TLifecycleExecutions` and `TFeatureEventRegistry` inside the immutable `TAbilityRules` that a battle shares.
- `CollectAbilities(value, catalog)` traverses current generated fields into a fresh `TAbilityCollector`. It expands features recursively, rejects cycles, deduplicates by definition plus source path while keeping provenance, and publishes nothing on failure. There is no second stored action list.
- `EnumerateActions` is pure: no prompts, RNG, resource writes or ID allocation. `ExecuteAbility` recollects grants and rechecks requirements, eligibility and costs before paying. Options anchor the actor by player address, so options from another battle with reused IDs are rejected.
- `RequireGrantImplementation` maps each action grant to one action execution and each reaction grant to one typed reaction; anything else is a configuration error.
- Source validation runs before the body and before every resumed continuation. A lost source raises `TAbilityUnavailable`; spent costs are not refunded.
- Entities, players and the battle must stay alive and at stable addresses while bound, journaled or referenced by savepoints.

## Compiled actions (`actions/generated_action.h`)
- Generated actions use `TAction::TCompiledBody`. The action context carries the acting player in `actor`. Costs bind generated `Resource` paths, are validated together and paid once atomically before the body.
- Entity bindings keep type, identity and participant address, and every read, guard and resume revalidates them. Readonly and mutable aliases compare equal but keep their capabilities.
- `IInteractionSystem::RequestChoice` either calls its receiver synchronously or suspends through `TSavepointStackUnwind`; returning without either is an error.
- `SelectActionValue`, `SelectActionEntity` and `SelectActionSource` return immediate choices directly and run their continuation only on resume, so long chains of immediate choices do not grow the stack. Replays revalidate the actor, candidate, predicate and granting source.
- A failed body keeps its paid costs and completed mutations in the caller's journal until the caller undoes them. Only a leading choice gets target-existence preflight.

## Sources (`ability_source.h`, `actions/generated_source.h`)
- `VisitAbilitySources` exposes stored class providers under grant paths, and `FindAbilitySource<T>` picks the nearest matching ancestor.
- An action `using TWeapon Weapon` checks its source before payment and prepares an immutable snapshot. Replacing a provider at the same path can keep eligibility; removing the grant or provider invalidates the continuation.
- `SourceCandidates` keep an owner binding and an exact entry locator, so a missing entry never falls back to another provider. Selection snapshots the value only after its predicate passes again.
- `ForkActionInputs` adds immutable input validators for a callee while sharing the actor, services and journal. Validators must use membership-only checks, because full actor validation from inside one recurses. The constraints end when the callee returns.

## Reaction windows (`actions/reaction_execution.h`, `actions/reaction_window.h`)
- `TReactionRegistry` and `TFeatureEventRegistry` validate their values and store them in `TTaggedRegistry` (`tagged_registry.h`), keyed by tag ID and definition. Conflicting tag types or values are rejected.
- `OpenReactionWindow` visits participants in insertion order, once per window. It offers decline plus eligible reactions and rechecks identity, grant, requirement and resources before preparation and atomic payment.
- A nonempty `LimitKey` records the reactor in the returned `TReactionScope`; `once per activity` triggers use their trigger ID as the key. Each invoked activity starts with a fresh scope.
- Offer and execution contexts are separate and do not inherit the parent's grant or input validators. Nested windows carry a trigger trace that rejects re-entering an active trigger.
- Window frames copy the pending value, arguments, scope and cursor, so suspended reactions replay without paying twice. Insertion order is the native policy, not full PF2e simultaneous-trigger resolution.

## Lifecycle, events and world (`actions/lifecycle_execution.h`, `feature_event.h`, `actions/generated_world.h`)
- `TAbilityRules::Lifecycle` binds round start, turn-start expiration, turn start and turn end to activities. Without a hook the battle calls the generated creature method. Lifecycle contexts run in `System` mode so inactive actors can still transition; ordinary actions never inherit it. `RunLifecycleAndDrain` replays successive savepoints with the original journal without rerolling.
- Feature events dispatch to the receiver's current `Features()`, once per definition and source, and return a snapshot of contributions with provenance. Handlers are synchronous and pure; nested dispatch and suspension are rejected.
- `TGeneratedWorldView` borrows a battle and checks each operand's identity and membership. `Entities<T>()` includes dead participants. Grid distance is Chebyshev distance in cells, and line of effect currently always succeeds. Unbound views allow readonly binding but reject spatial queries.

## Records, relations and time (`actions/action_history.h`, `actions/generated_history.h`, `game_time.h`, `actions/generated_relation.h`)
- The battle owns a journaled per-actor `TActionHistory`. Accepting an own root action or reaction replaces that actor's receipt in the same transaction as payment; failed availability or payment leaves it intact.
- Records store immutable source snapshots and detached entity locators, never an old context. Each acceptance allocates a version object that options and tickets compare, so an abandoned rollback branch cannot become valid again.
- `TGameClock` advances once per completed initiative round by the authored round duration. A timed relation is active while `now < deadline`, and reads never prune. Clock and relations are encounter-only state.

## Generated method execution (`generated_execution.h`)
- `TGeneratedExecutionContext` wraps the `TTransformator` journal. Generated methods open `ctx.Atomic()`, write through `ctx.Assign`, and commit on success; a failure undoes everything since the checkpoint.
- `ctx.Invoke`, `ctx.Project` and `ctx.AssignGuarded` reach nested fields through `TGuarded<T>` without exposing mutable references. Variant sets and resources have checked journaled helpers.
- Journaled fields must stay at stable addresses while their history exists. Do not journal writes into collection elements and then replace the collection.
- Transactions are synchronous: methods cannot prompt, yield or open reaction windows.

## Identity, collections and construction
- `NewEntityId` uses a mutex-protected, process-wide counter that is not journaled.
- `TIdCollection<T>` IDs only increase. `RestoreEntry` followed by `RestoreNextId` builds a collection with explicit IDs.
- `construction.h` holds constructor reports. `generated_construction.h` creates private storage for generated construction without running rule defaults.
- `actions/generated_initiative.h` moves a participant before the current turn through the journal and applies no game policy.

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

# pf2e_engine common utilities

Canonical documentation file. AGENTS.md must remain semantically equivalent.

Shared low-level building blocks used across the engine. Two pieces carry
non-obvious design constraints worth knowing before changing them.

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

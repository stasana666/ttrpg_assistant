# DSL runtime (parser wrapper + evaluator)

The runtime side of the action-pipeline expression DSL. The lexer/parser/grammar
are **not** here — they come from the shared expression front-end
([tools/common/expr/](../../../../tools/common/expr/)). This directory owns the
thin wrapper plus everything needed to evaluate the result:
- [parser.cpp](parser.cpp) — `ParseDsl(src)` wraps `expr::Parse` into a `TDslExpression`.
- [evaluator.cpp](evaluator.cpp) — `TDslExpression::Evaluate(TEvalContext&)` walks
  the shared `expr::TExprNode` against the property/function registries.
- [property_access.cpp](property_access.cpp), [function_registry.cpp](function_registry.cpp),
  [builtins.cpp](builtins.cpp) — registries and the initial property/function bindings.
- [value_convert.cpp](value_convert.cpp), [scope_guard.cpp](scope_guard.cpp) — `TDslValue`
  conversion and scoped `$item`/`$acc` binding.

See the root [CLAUDE.md](../../../../CLAUDE.md) "DSL Expression Layer" section for the
grammar, registries, and the list of built-in properties/functions.

Implementation details that are easy to get wrong:

## `TDslValue` and the `bool` constraint (`value.h`)

`TDslValue` is the polymorphic value type used inside DSL expressions. It is kept
separate from `TGameObjectPtr` so that adding DSL-only alternatives (bool, generic
lists) does not ripple into the dozens of `std::visit` sites on the registry-side
variant. Lists are reference-counted (`shared_ptr<const TList>`) to keep `TDslValue`
cheap to copy.

The `bool` constructor is **constrained with `requires std::same_as<B, bool>`** so
that pointers do NOT silently coerce to `bool` via implicit pointer-to-bool
conversion (which would otherwise outrank the pointer overloads when only a
`const T*` is available at the call site, putting the wrong alternative into the
variant). If you add a new pointer alternative, prefer non-const access at the call
site rather than introducing const-pointer constructors.

## Property registry (`property_registry.h`)

Per-class `string → getter` map. Dispatch is done by visiting the receiver's variant
and looking up its `TPropertyRegistry<T>` specialization. Getters are stored as
type-erased lambdas because getters in this codebase have heterogeneous return
types (`int`, `const T&`, `vector<T>`, …) — member pointers cannot share a single
map type without that type erasure. The `Getter<T, R, M>()` helper wraps a
uniform-signature member-function pointer when it applies, eliminating the lambda
boilerplate at the call site; otherwise write the lambda inline.

## Scope binding (`expression.h`)

`filter`/`map`/`foldl` bind `$item` (and `$acc` for `foldl`) into
`TEvalContext::scope`, an `unordered_map` that **shadows registry entries for the
duration of the scope**. `TScopeGuard` is an RAII helper that pushes a value into
`ctx.scope[name]` for the guard's lifetime, then restores the prior binding (or
removes it if there was none). Because it saves and restores, nested
filters/maps/folds with the same variable name correctly nest and unwind without
mutating the registry. `TDslExpression` is a thin wrapper around the shared
`expr::TExprNode` AST plus this evaluator — it replaced the former `IDslExpression`
node hierarchy once parsing moved into the shared `expr` front-end.

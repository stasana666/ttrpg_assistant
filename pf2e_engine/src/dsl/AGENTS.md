# Runtime DSL

## Purpose
- Runtime expression evaluation for action JSON and engine behavior.
- This is not the `.ttrpg` schema/codegen language; it shares the generic `expr` parser but has separate evaluation, values, functions, and properties.

## Key Files
- `parser.cpp`: wraps `expr::Parse` into `TDslExpression`.
- `evaluator.cpp`, `value_convert.cpp`, and `include/pf2e_engine/dsl/value.h`: runtime values and evaluation.
- `function_registry.cpp`, `builtins.cpp`: callable DSL functions.
- `property_access.cpp`: object property lookup, including generated schema properties.
- Tests live in `pf2e_engine/tests/dsl/`.

## Concepts
- Generic action blocks `let`, `filter`, `map`, and `foldl` evaluate DSL expressions with scoped `$item` and `$acc` bindings.
- `TDslValue` is separate from game-object variants; its `bool` constructor is constrained to avoid accidental pointer-to-bool conversion.
- `TPropertyRegistry<T>` stores type-erased getters; prefer registering properties/functions in `builtins.cpp` over adding bespoke blocks.

## Build/Test
- Build/run DSL tests: `cmake --build build --target test_dsl`.
- For action behavior that uses DSL expressions, also check `test_actions` or `test_wolf_combat`.

## Pitfalls
- Keep `$self`, `$item`, `$acc`, and other action-pipeline variables scoped through evaluation context.
- Adding DSL-visible fields may require generated `RegisterDslProperties()` support from `.ttrpg` codegen.
- Do not add PF2E-specific parser syntax here unless the generic `tools/common/expr` users can also tolerate it.
- Burst/cone/line targeting still uses `get_targets_in_area`; the DSL has no `TPosition` value or position-picking function.

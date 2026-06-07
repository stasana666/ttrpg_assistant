# Engine Tests

Canonical documentation file. AGENTS.md must remain semantically equivalent.

## Purpose
- GoogleTest coverage for engine mechanics, actions, runtime DSL, inventory data, game object loading, AST snapshots, and transformations.

## Key Files
- `test_lib/mock_dice_roller.h`: deterministic RNG expectations for reproducible tests.
- `test_lib/mock_interaction_system.h`: scripted player choices and optional state checks.
- `actions/`: combat/action pipeline tests using real data.
- `inventory/`: generated schema type and JSON loading tests.
- `dsl/`: runtime DSL parser/evaluator/property tests.
- `ast/`: rollback and AST state-comparison tests.

## Build/Test
- Build a focused target, for example `cmake --build build --target test_inventory`.
- Run all registered tests: `ctest --test-dir build --output-on-failure`.
- `test_actions` is built but is not registered with CTest in current CMake; run `./build/pf2e_engine/tests/actions/test_actions` directly for action-combat changes.
- Many tests load `pf2e_engine/data` using generated `cpp_config.h` and `kRootDirPath`.

## Pitfalls
- Some combat tests intentionally stop long battle loops by expecting `TTooManyCallsError`.
- Keep expected RNG rolls and interaction choices in exact sequence.
- When adding data-dependent tests, add only minimal fixture data or reuse existing JSON sources.

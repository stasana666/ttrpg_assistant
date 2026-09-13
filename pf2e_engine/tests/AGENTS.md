# Engine Tests

Canonical project instructions. CLAUDE.md redirects here.

## Purpose
- GoogleTest coverage for engine mechanics, actions, runtime DSL, inventory data, game object loading, AST snapshots, and transformations.

## Key Files
- `test_lib/mock_dice_roller.h`: deterministic RNG expectations for reproducible tests.
- `test_lib/mock_interaction_system.h`: scripted player choices and optional state checks.
- `actions/`: combat/action pipeline tests using real data.
- `inventory/`: generated schema type and JSON loading tests.
- `inventory/fixtures/rule_entities.ttrpg` and `inventory/test_rule_entities.cpp`:
  compile generated entities into `test_inventory` and verify named factories,
  identity transfer, nested methods, rollback, pure queries, and snapshots with
  collection allocator state.
- `inventory/fixtures/granted_entities.ttrpg` imports `granted_abilities.ttrpg`;
  `inventory/test_granted_entities.cpp` verifies imported ability registration,
  feature and weapon grant traversal, action/reaction distinction, source IDs,
  and provenance preservation through snapshots and removal of one source.
- `dsl/`: runtime DSL parser/evaluator/property tests.
- `ast/`: rollback and AST state-comparison tests.

## Build/Test
- Build a focused target, for example `cmake --build build --target test_inventory`.
- Run all registered tests: `ctest --test-dir build --output-on-failure`.
- `test_actions` is registered with CTest through `gtest_discover_tests`; it can also run directly as `./build/pf2e_engine/tests/actions/test_actions`.
- Many tests load `pf2e_engine/data` using generated `cpp_config.h` and `kRootDirPath`.

## Pitfalls
- Some combat tests intentionally stop long battle loops by expecting `TTooManyCallsError`.
- Keep expected RNG rolls and interaction choices in exact sequence.
- When adding data-dependent tests, add only minimal fixture data or reuse existing JSON sources.

# Engine Tests

Canonical project instructions. CLAUDE.md redirects here.

## Purpose
- GoogleTest coverage for engine mechanics, actions, runtime DSL, inventory data, game object loading, AST snapshots, and transformations.

## Key Files
- `test_lib/mock_dice_roller.h`: deterministic RNG expectations for reproducible tests.
- `test_lib/mock_interaction_system.h`: scripted player choices and optional state checks.
- `actions/`: combat/action pipeline tests using real data.
- `actions/fixtures/generated_actions.ttrpg` and `actions/test_generated_actions.cpp`
  compile actions into `test_generated_actions` and execute them through battle
  grants. They cover chosen entities, dice, arbitrary declared resource fields,
  HP effects, suspended and repeated choices, source removal, actor eligibility,
  and caller-controlled journal rollback. These fixtures are language acceptance
  cases, not the production PF2e action catalog.
- `actions/test_ability_runtime.cpp` covers grant and feature provenance, immutable
  hook execution, source removal during suspension, replay after successful resume
  and rollback, and battle identity/insertion failures. Hook-removal tests replace
  a generated class value through the journal only when none of its child fields
  have retained mutation history; they do not introduce collection mutation APIs.
- `actions/test_feature_event_runtime.cpp` builds as `test_feature_events` and
  checks typed immutable handler registration, readonly argument capabilities,
  source deduplication and provenance, snapshot results, context membership,
  stale entity bindings, and rejection of nested or suspending event handlers.
- `actions/test_source_binding_runtime.cpp` builds as `test_source_bindings` and
  checks nearest typed source resolution, immutable execution registration,
  preparation before payment, readonly source snapshots, stable-path replacement,
  actor/source invalidation and repeated resume checks before native callbacks.
  Collection replacement cases retain no earlier child-field journal entries.
- `actions/test_world_runtime.cpp` checks readonly world capabilities, participant
  identity and membership, unbound-world failures, coordinates and the explicit
  Chebyshev grid metric, live geometry across rollback, world-aware preflight before
  payment, legacy requirement adapters, and world propagation into pure events.
- `actions/fixtures/generated_sources.ttrpg` and `actions/test_generated_sources.cpp`
  compile into `test_generated_sources`. They verify named equipment definitions,
  map/set and trait-payload construction, readonly class/collection queries,
  same-name weapon sources, natural weapons, source preflight, and snapshot values
  across removal, replacement, suspension and rollback.
- `actions/fixtures/generated_world.ttrpg` and `actions/test_generated_world.cpp`
  compile into `test_generated_world`. They check target existence before payment,
  live positions, teams and participants in action predicates and pure feature
  events, source reach snapshots, and suspended target validation with rollback.
  Prefix `requires` gates new executions before payment; a paid continuation does
  not rerun it. Existing imperative `require` keeps its body assertion semantics.
  The ally-count fixture is a language test, not a production PF2e action. Its
  line-of-effect check uses the current map adapter, which always reports true.
- `inventory/`: generated schema type and JSON loading tests.
- `inventory/fixtures/rule_entities.ttrpg` and `inventory/test_rule_entities.cpp`:
  compile generated entities into `test_inventory` and verify named factories,
  identity transfer, nested methods, rollback, pure queries, and snapshots with
  collection allocator state.
- `inventory/fixtures/granted_entities.ttrpg` imports `granted_abilities.ttrpg`;
  `inventory/test_granted_entities.cpp` verifies imported ability registration,
  feature and weapon grant traversal, action/reaction distinction, source IDs,
  and provenance preservation through snapshots and removal of one source.
- `inventory/test_proficiency.cpp` verifies generated rank and fixed-value queries,
  map defaults, enum JSON, snapshots, and checked arithmetic. The mechanics tests
  in `mechanics/test_proficiency_calculation.cpp` cover real warrior/wolf bonuses,
  level changes with rollback, fixed contributions, and condition penalties.
- `game_object_logic/test_proficiency_loading.cpp` covers legacy proficiency
  normalization, canonical nested profiles, missing defaults, conflicting input
  locations, invalid enum keys, and integer bounds through the real factory.
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

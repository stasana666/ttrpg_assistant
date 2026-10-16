# Engine Tests

Canonical project instructions. CLAUDE.md redirects here.

## Purpose
- GoogleTest coverage for engine mechanics, actions, runtime DSL, inventory data, game object loading, AST snapshots, and transformations.

## Shared Infrastructure
- `test_lib/mock_dice_roller.h`: deterministic RNG expectations for reproducible tests.
- `test_lib/mock_interaction_system.h`: scripted player choices and optional state checks for legacy synchronous tests.
- `test_lib/scripted_io.h`: `TScriptedIo`, the shared scripted choice mock for generated actions. Each expectation names the actor, optional alternatives kind, a name or typed value matcher, and whether the choice suspends through a savepoint. It records requests, suspensions, nesting depth, legacy reaction calls and the last candidate names per kind.
- `test_lib/battle_fixture.h`: `TBattleFixture` owns the battle, separate journal, RNG, IO and creature storage, and provides player lookup, contexts, option lookup with an optional id prefix, source-validated `Execute`, `Pause` and AST snapshots. `TGeneratedCombatFixture` uses the real factory rules. Test fixtures derive from it and keep only domain-specific creature builders.
- `CMakeLists.txt` defines `add_engine_test(name SOURCES ... [SCHEMAS ...] [INCLUDES ...])`; schemas are generated into the shared inventory output directory.
- Many tests load `pf2e_engine/data` using generated `cpp_config.h` and `kRootDirPath`.

## Layout
- `actions/test_generated_<area>.cpp` with `actions/fixtures/generated_<area>.ttrpg` are language acceptance tests for compiled actions, sources, world queries, control flow, activities, source choices, reactions, records and relations. The fixtures are generic language cases, not the production PF2e catalog.
- `actions/test_generated_{strike,trip,shield,movement,intimidation,flanking}.cpp` run the production rule modules. `test_generated_encounter` runs `TBattle::StartBattle` with the Remaster fighter and two wolves to both winners and replays from a round checkpoint.
- `actions/test_*_runtime.cpp`, `test_action_history.cpp` and `test_time_relations.cpp` exercise native runtime pieces directly with hand-written executions.
- `actions/test_action_combat.cpp`, `test_mixed_attack_runtime.cpp`, `test_legacy_damage_context.cpp` and `test_wolf_combat.cpp` cover legacy JSON pipelines and their interaction with compiled rules.
- `test_lifecycle*.cpp`, `test_initiative.cpp`, `test_knockout_initiative.cpp` and `test_health.cpp` cover battle phases, initiative ordering and relocation, and health transitions.
- `inventory/` covers generated schema types, JSON loading, configurable factories, grants and Remaster profiles; `mechanics/`, `game_object_logic/`, `dsl/`, `ast/` and `transformation/` cover calculators, factory loading, the runtime DSL, AST rollback comparison and journal transformations.

## Build/Test
- Build a focused target, for example `cmake --build build --target test_inventory`.
- Run all registered tests: `ctest --test-dir build --output-on-failure`.

## Pitfalls
- Some combat tests intentionally stop long battle loops by expecting `TTooManyCallsError`.
- Keep expected RNG rolls and interaction choices in exact sequence.
- When adding data-dependent tests, add only minimal fixture data or reuse existing JSON sources.
- Replacing a generated class value or collection through the journal is only safe when none of its child fields have retained mutation history; tests make such edits in a separate journal.
- Rollback tests compare full battle and journal AST snapshots. Keep observer counters and checkpoints outside the compared game state.

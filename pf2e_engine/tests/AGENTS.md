# Engine Tests

Canonical project instructions. CLAUDE.md redirects here.

## Purpose
- GoogleTest coverage for engine mechanics, actions, generated rule types, AST snapshots, and transformations.

## Shared Infrastructure
- `test_lib/mock_dice_roller.h`: deterministic RNG expectations for reproducible tests.
- `test_lib/mock_interaction_system.h`: scripted player choices and optional state checks for legacy synchronous tests.
- `test_lib/scripted_io.h`: `TScriptedIo`, the shared scripted choice mock for generated actions. Each expectation names the actor, optional alternatives kind, a name or typed value matcher, and whether the choice suspends through a savepoint. It records requests, suspensions, nesting depth and the last candidate names per kind.
- `test_lib/journal_setup.h`: `SetCondition`, `AddResource` and `ReduceResource` change one condition or resource through a journaled generated assignment, for tests that need a starting state.
- `test_lib/warrior.h`: `MakeWarrior()` builds the level 1 warrior that older engine tests use, in code with fixed scores, proficiency and a d6 longsword.
- `test_lib/battle_fixture.h`: `TBattleFixture` owns the battle, separate journal, RNG, IO and creature storage, and provides player lookup, contexts, option lookup with an optional id prefix, source-validated `Execute`, `Pause` and AST snapshots. `TGeneratedCombatFixture` uses the production rules from `CreateAbilityRules()`. Test fixtures derive from it and keep only domain-specific creature builders. Free helpers `Features` and `Fixed` build feature selections and fixed baselines.
- `Act(option, step)` runs one action described by a `TStep`: choices in order (`TPick` by label, by actor and label, or `TPick::Value`), dice as `{size, result}` pairs, the expected `Result`, and `Changes`, the complete map of tracked fields that must change. Paths start with the player name for creature fields, `position` and `team`, or with the battle field name; values are written without the AST type prefix. Action history, relations, the journal and the map are not tracked, so tests check them through their queries. Empty objects are not tracked either, a variant entry such as a condition is tracked as its own path with value `true`, and a vanished path has the value `<removed>`. Every step also checks that enumeration is pure, that the action consumed exactly the listed dice and choices, that undo restores the snapshot, and, for applied actions, that suspending at every asynchronous choice, reverting each savepoint and resuming reach the same final state. `Check` does the same and then restores the state from before the step. Prefer `Act` over hand-written snapshot, undo and replay assertions.
- `test_lib/state_diff.h` holds the field comparison shared by `Act` and `ExpectStep(object, journal, operation, changes)`, which runs a journaled operation on one generated object, compares its changed fields with `changes`, then checks that undo restores the object and journal and that repeating the operation reaches the same state. Unit tests of generated rules use it instead of hand-written snapshot and undo assertions. `ObjectFields(object)` gives the tracked fields of one object, for checking that a query left it unchanged.
- `CMakeLists.txt` defines `add_engine_test(name SOURCES ... [SCHEMAS ...] [INCLUDES ...])`; schemas are generated into the shared inventory output directory.

## Layout
- `actions/test_generated_<area>.cpp` with `actions/fixtures/generated_<area>.ttrpg` are language acceptance tests for compiled actions, sources, world queries, control flow, activities, source choices, reactions, records and relations. The fixtures are generic language cases, not the production PF2e catalog.
- `actions/test_generated_{strike,trip,shield,movement,intimidation,flanking,spells}.cpp` run the production rule modules. `test_generated_encounter` runs `TBattle::StartBattle` with the Remaster fighter and two wolves to both winners and replays from a round checkpoint.
- `actions/test_*_runtime.cpp`, `test_action_history.cpp` and `test_time_relations.cpp` exercise native runtime pieces directly with hand-written executions.
- `test_lifecycle*.cpp`, `test_initiative.cpp`, `test_knockout_initiative.cpp` and `test_health.cpp` cover battle phases, initiative ordering and relocation, and health transitions.
- `inventory/` covers generated schema types, configurable constructors, grants and Remaster profiles; `mechanics/`, `ast/` and `transformation/` cover generated statistics and combat rules, AST rollback comparison and journal transformations.

## Build/Test
- Build a focused target, for example `cmake --build build --target test_inventory`.
- Run all registered tests: `ctest --test-dir build --output-on-failure`.

## Pitfalls
- Some combat tests intentionally stop long battle loops by expecting `TTooManyCallsError`.
- Keep expected RNG rolls and interaction choices in exact sequence.
- Build test creatures with generated constructors or `test_lib/warrior.h`.
- Replacing a generated class value or collection through the journal is only safe when none of its child fields have retained mutation history; tests make such edits in a separate journal.
- Rollback tests compare full battle and journal AST snapshots. Keep observer counters and checkpoints outside the compared game state.

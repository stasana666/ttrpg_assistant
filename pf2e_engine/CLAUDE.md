# PF2E Engine

Canonical documentation file. AGENTS.md must remain semantically equivalent.

## Purpose
- Core headless PF2E game-mechanics engine: battle state, maps, players, resources, actions, mechanics, inventory, runtime DSL, transformations, continuations, and AST state comparison.
- Public headers live in `include/pf2e_engine/`; implementations live in `src/`; data lives in `data/`; tests live in `tests/`.
- Keep this code independent of assistant GUI/audio and analyzer strategy implementations.

## Key Concepts
- Target `pf2e_engine` links `nlohmann_json`, `nlohmann_json_schema_validator`, `ttrpg_parse`, and `expr`.
- `TBattle` orchestrates rounds, turns, initiative, resources, effects, scheduled tasks, transformations, and battle end state.
- The engine talks to outside systems only through `IInteractionSystem`; assistant, analyzer, and tests provide concrete implementations.
- `TGameObjectFactory` loads JSON definitions recursively from `pf2e_engine/data` and creates typed objects by registered ids.
- Action JSON pipelines are parsed by `TActionReader`; action-block functions are registered in `src/actions/action_reader.cpp`.
- Runtime DSL expressions in action data are evaluated by `src/dsl/`; parsing comes from shared `tools/common/expr`.
- Inventory, condition, and creature-data support types are generated from `.ttrpg` schemas into `build/generated/pf2e_engine/...`.
- Rollback/resume behavior depends on `TTransformator`, `TSavepointStackUnwind`, continuation helpers, and AST state comparison.

## Build/Test
- Build engine: `cmake --build build --target pf2e_engine`.
- Common test targets: `test_pf2_engine`, `test_inventory`, `test_dsl`, `test_actions`, `test_wolf_combat`, `test_mechanics`, `test_expressions`, `test_game_object_logic`, `test_transformation`, `test_ast_state`.
- Broad check: `ctest --test-dir build --output-on-failure`.
- `test_actions` is built but not registered with CTest in current CMake; run `./build/pf2e_engine/tests/actions/test_actions` directly for action-combat changes.

## Extending Actions
- Prefer exposing a DSL property or function in `src/dsl/builtins.cpp` before adding a new action-block class.
- Add a new action block only for C++ side effects such as RNG, transformations, scheduling, prompts, or named outputs beyond `let`.
- When adding a block, update the header/source, `pf2e_engine/src/action_blocks/CMakeLists.txt`, includes, and `TPipelineReader::kFunctionMapping`.

## Pitfalls
- Do not edit generated headers/sources in `build/generated`; edit schemas or codegen instead.
- Data-loading tests depend on generated `cpp_config.h` and `kRootDirPath`; avoid hard-coded absolute paths.
- Mutations that bypass `TTransformator` can survive rollback and should be caught by AST tests.
- Use local docs under `data/`, `data/schemas/`, `src/dsl/`, `include/pf2e_engine/common/`, and `tests/` before loading unrelated engine context.

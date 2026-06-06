# PF2E Engine

## Purpose
- Core PF2E game engine library: battle state, map, players, actions, resources, mechanics, inventory, runtime DSL, AST snapshots, and transformations.
- Public headers are under `include/pf2e_engine/`; implementations are under `src/`; game data and schemas are under `data/`; tests are under `tests/`.

## Key Concepts
- `pf2e_engine` links `nlohmann_json`, `nlohmann_json_schema_validator`, `ttrpg_parse`, and `expr`.
- `TGameObjectFactory` loads all JSON data recursively and creates typed game objects by registered ids.
- The engine talks to outside systems through `IInteractionSystem`; assistant, analyzer, and tests provide concrete implementations.
- Action JSON pipelines are read by `TActionReader` and use action-block functions registered in `src/actions/action_reader.cpp`.
- The runtime DSL in `src/dsl/` evaluates expressions in action data. It is separate from the `.ttrpg` schema/codegen language.
- Inventory and condition types are generated from `.ttrpg` schemas into `build/generated/pf2e_engine/...`.
- Rollback/resume behavior depends on `TTransformator`, AST state comparison, and continuation helpers in `include/pf2e_engine/common/`.

## Build/Test
- Build engine: `cmake --build build --target pf2e_engine`.
- Common test targets: `test_pf2_engine`, `test_inventory`, `test_dsl`, `test_actions`, `test_wolf_combat`, `test_mechanics`, `test_expressions`, `test_game_object_logic`, `test_transformation`, `test_ast_state`.
- Broad check: `ctest --test-dir build --output-on-failure`.

## Pitfalls
- Do not edit generated headers/sources in `build/generated`.
- Prefer adding DSL properties/functions before adding new action-block classes; add blocks only for side effects, prompts, RNG, or named outputs.
- When adding an action-block function, update its source/header, CMake lists, and the function map in `TActionReader`.
- Data-loading tests depend on `cpp_config.h` and `kRootDirPath`; avoid hard-coded absolute paths.
- Keep engine code independent of assistant GUI/audio and analyzer strategy code.

# PF2E Engine

Canonical project instructions. CLAUDE.md redirects here.

## Purpose
- Core headless PF2E game-mechanics engine: battle state, maps, players, resources, actions, mechanics, inventory, runtime DSL, transformations, continuations, and AST state comparison.
- Public headers live in `include/pf2e_engine/`; implementations live in `src/`; data lives in `data/`; tests live in `tests/`.
- Keep this code independent of assistant GUI/audio and analyzer strategy implementations.

## Key Concepts
- Target `pf2e_engine` links `nlohmann_json`, `nlohmann_json_schema_validator`, `ttrpg_parse`, and `expr`.
- `TBattle` orchestrates rounds, turns, initiative, resources, effects, scheduled tasks, transformations, and battle end state.
- The engine talks to outside systems only through `IInteractionSystem`; assistant, analyzer, and tests provide concrete implementations.
- `TGameObjectFactory` loads JSON definitions recursively from `pf2e_engine/data` and creates typed objects by registered ids.
- New generated types register with `TGameObjectFactory::RegisterType<T>(jsonType)` before loading by-name definitions. Inline generated classes need no registration; generic definitions are immutable JSON and resolve nested references through the factory performing `Create<T>`.
- Action JSON pipelines are parsed by `TActionReader`; action-block functions are registered in `src/actions/action_reader.cpp`.
- Runtime DSL expressions in action data are evaluated by `src/dsl/`; parsing comes from shared `tools/common/expr`.
- Creature, inventory, and condition types are generated from `.ttrpg` schemas into `build/generated/pf2e_engine/...`.
- Hit-point storage and damage/healing methods are generated from `data/schemas/hitpoints.ttrpg`; `TTransformator` delegates to these methods through `TGeneratedExecutionContext`. It does not implement hit-point clipping or temporary-hit-point rules.
- Rollback/resume behavior depends on `TTransformator`, `TSavepointStackUnwind`, continuation helpers, and AST state comparison.
- Generated `VisitGrantedAbilities` derives ability grants from current creature, race, class, weapon, and natural-weapon feature selections. `TCreature` has no separately mutable action or reaction list; the battle owns shared immutable `TAbilityRules` with definitions and execution bindings.
- `TBattle::AvailableActions`, `AvailableReactions(trigger)`, and `Execute(option)` expose bound choices. Sources use generated field paths plus collection entry IDs. Execution checks the live grant and cost again; suspended actions retain their definition and weapon snapshot and revalidate their source before resuming.
- `.ttrpg` action bodies compile directly to C++ with resource costs, entity choices, dice, and journaled field writes/method calls. `Register<Module>Rules` binds their executors and pure typed feature-event handlers alongside the catalog. Production PF2e actions still use JSON pipelines during migration; source-bound compiled combat, reaction bodies, and reaction windows remain subsequent work.
- `data/schemas/proficiency.ttrpg` generates proficiency storage and rank/fixed-bonus rules. `TCreature` owns `Proficiency`; its queries pass the creature's current `Level`, so no separate proficiency level is stored. The remaining C++ `ProficiencyBonus` overloads only route calculator requests to these generated queries.
- `TCreature` is a generated `GameEntity` with no handwritten subclass. Legacy `TCreatureFeat` pipeline hooks remain an import adapter for Pack Attack; they are immutable ruleset definitions selected through generated features. Remaining native resource/condition/reach adapters live in `mechanics/creature.h`; action bodies, contextual modifiers, character choices, and PF2e reaction timing are subsequent rule-language work.

## Build/Test
- Build engine: `cmake --build build --target pf2e_engine`.
- Common test targets: `test_pf2_engine`, `test_inventory`, `test_dsl`, `test_actions`, `test_wolf_combat`, `test_mechanics`, `test_expressions`, `test_game_object_logic`, `test_transformation`, `test_ast_state`.
- Broad check: `ctest --test-dir build --output-on-failure`.
- `test_actions` is registered with CTest through `gtest_discover_tests`; it can also run directly as `./build/pf2e_engine/tests/actions/test_actions`.

## Extending Actions
- Prefer compiled `.ttrpg` bodies for new declarative rules. Extend compiler operations through small generic runtime adapters; keep PF2e formulas in rule declarations.
- For legacy JSON pipelines, prefer exposing a DSL property or function in `src/dsl/builtins.cpp` before adding a new action-block class.
- Add a new action block only for C++ side effects such as RNG, transformations, scheduling, prompts, or named outputs beyond `let`.
- When adding a block, update the header/source, `pf2e_engine/src/action_blocks/CMakeLists.txt`, includes, and `TPipelineReader::kFunctionMapping`.

## Pitfalls
- Do not edit generated headers/sources in `build/generated`; edit schemas or codegen instead.
- Data-loading tests depend on generated `cpp_config.h` and `kRootDirPath`; avoid hard-coded absolute paths.
- Mutations that bypass `TTransformator` can survive rollback and should be caught by AST tests.
- Use local docs under `data/`, `data/schemas/`, `src/dsl/`, `include/pf2e_engine/common/`, and `tests/` before loading unrelated engine context.

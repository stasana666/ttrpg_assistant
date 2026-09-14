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
- Hit-point storage and clipping are generated from `data/schemas/hitpoints.ttrpg`. Creature health rules add knockout, dying, recovery, healing and death using the types in `health.ttrpg`. `TTransformator` delegates to the generated creature methods through `TGeneratedExecutionContext`; it does not implement those rules. Legacy damage expressions carry the generated critical/nonlethal context to the same delivery method.
- Rollback/resume behavior depends on `TTransformator`, `TSavepointStackUnwind`, continuation helpers, and AST state comparison.
- Scheduler rollback restores removed tasks in their original creation order. Restored IDs must have been allocated, be absent from the current queue, and carry valid event progress. Callback order is part of the state verified by the lifecycle tests.
- Initiative entries retain their original roll totals, bonuses and tie metadata in an explicitly ordered list. Journaled relocation preserves the current iterator and restores each moved participant before its previous successor on Undo. Enrollment is restricted to setup; rollback to before the first round restores setup. The generated `move_before_current_turn(Entity)` operation validates actor, source and writable battle membership, then uses the current turn rather than the executing actor as its anchor. It has no health policy and is a validated no-op when no turn is active.
- Generated `VisitGrantedAbilities` derives ability grants from current creature, race, class, weapon, and natural-weapon feature selections. `TCreature` has no separately mutable action or reaction list; the battle owns shared immutable `TAbilityRules` with definitions and execution bindings.
- `TBattle::AvailableActions`, `AvailableReactions(trigger)`, and `Execute(option)` expose bound choices. Sources use generated field paths plus collection entry IDs. Execution checks the live grant and cost again; suspended actions retain their definition and weapon snapshot and revalidate their source before resuming.
- `TActionHistory` is battle-owned, journaled state for declared completed-action
  records. Accepted own actions and reactions invalidate the previous result;
  subordinate activities and system phases leave it unchanged. Record consumers
  prepare detached source/entity inputs before acceptance clears the receipt.
  Immutable version stamps protect saved options and terminal publication across
  intervening reactions and divergent rollback branches. Turn boundaries add no
  implicit expiration policy.
- `TGameClock` and `TTimedRelationStore` keep encounter time and declared entity-pair
  relations in the battle journal. The factory reads round duration from the named
  `.ttrpg` combat timing value; native code supplies no PF2e duration. A completed
  initiative round advances time once, while setup and partial-round victory do not.
  Timed relations expire during pure reads; permanent values have no deadline.
  Neither clock nor relation state is copied by creature snapshots or new battles.
- `.ttrpg` action bodies compile directly to C++ with resource costs, entity choices, dice, and journaled field writes/method calls. `Register<Module>Rules` binds their executors and pure typed feature-event handlers alongside the catalog. Production melee Strike and Pack Attack use this path; other PF2e actions still use JSON pipelines during migration. Typed trigger declarations and reaction bodies compile to replayable windows; the author declares pending values, subscriptions, requirements and costs while the generator owns continuation frames and activity scopes. Production movement opens predeparture Reactive Strike windows; generated attack damage opens the Shield Block window after defenses and before temporary HP.
- Typed `.ttrpg` activities compose action behavior and return values across nested choices. They inherit the caller's actor, source validation and transformation journal; only the outer action pays its declared cost. Immediate calls return directly and suspended calls resume through copied continuation frames.
- Source choices select readonly snapshots from current stored collections, independently of the feature granting the action. Production Vicious Swing uses this mechanism with equipment and natural weapons, the shared melee activity and the declared Flourishes cost. Nested activities preserve source validity through scoped child contexts.
- Generated creature methods own round/turn resource replenishment, attack-counter resets and shield expiration. A generic turn-start expiration phase precedes scheduled start-turn effects. Optional `TAbilityRules::Lifecycle` hooks replace the synchronous boundary methods with generated activities; the factory binds turn start to `StartCreatureTurn` for recovery and feature contributions before replenishment. System execution permits automatic transitions of inactive actors while retaining identity and membership checks. Ordinary actions and reactions require `CanAct()`; encounter completion uses `IsDefeated()`. Flourish uses a shared per-round resource.
- `data/schemas/proficiency.ttrpg` generates proficiency storage and rank/fixed-bonus rules. `TCreature` owns `Proficiency`; its queries pass the creature's current `Level`, so no separate proficiency level is stored. The remaining C++ `ProficiencyBonus` overloads only route calculator requests to these generated queries.
- `TCreature` is a generated `GameEntity` with no handwritten subclass. Legacy `TCreatureFeat` pipeline hooks remain an import adapter for Pack Attack; they are immutable ruleset definitions selected through generated features. Remaining native resource/condition/reach adapters live in `mechanics/creature.h`; remaining action rules and contextual feature evaluation outside melee AC continue the migration. Production configurable fighter and wolf declarations live in remaster_profiles.ttrpg.
- Combat calculators delegate AC, attack bonuses, attack counting, check outcomes, damage defenses, Perception, saves and skills to `.ttrpg` rules. Generated Lore and Class DC profiles support independent proficiency and complete-statistic baselines. Explicit typed modifier inputs merge with conditions in the noncombat statistic queries; the native initiative adapter uses the generated Perception query; automatic feature evaluation for other statistic families remains separate work. The generated melee activity collects contextual AC contributions, including flanking, before rolling. Generated and legacy attacks share `AttacksMade`; the current weapon determines its MAP. Historical JSON attack names remain available as migration adapters.

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

## Declared Encounter Entry Points
- Production declarations are `data/schemas/remaster_profiles.ttrpg`: generated
  `CreateHumanFighter1(THumanFighter1Options)` and `CreateWolf(TWolfOptions)` create
  independent entities. Factory `AbilityRules()` registers their imported rule
  modules; `TBattle` enumerates current grants and runs generated actions.
- The headless acceptance scenario is available through
  `./build/analyzer/analyzer --scenario fighter-wolves --iterations 10`.
  Full encounter tests exercise both victory outcomes and exact round-boundary
  Undo/replay. The supported profile uses readied melee weapons on an open grid;
  remaining equipment, terrain, perception and catalogue boundaries are recorded
  in `data/schemas/AGENTS.md`.
- Production frightened writes use generated creature methods and declared turn
  decay. Direct legacy `TEffectManager::AddEffect(Frightened)` remains only in
  compatibility transformation tests; do not mix its private canceler state with
  generated fear methods or new battle rules.

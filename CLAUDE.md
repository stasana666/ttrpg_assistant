# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a tabletop RPG (TTRPG) assistant for D&D/Pathfinder (PF2e), written in C++23. It provides a game engine for managing combat encounters with support for multiple interfaces: GUI (SFML), CLI, and optional voice input (Vosk + llama.cpp).

The repository is split into three top-level components:

- **`pf2e_engine/`** — the core, headless game-mechanics engine. Built as the `pf2e_engine` library; links only `nlohmann_json` + `json-schema-validator`. No GUI/voice dependencies.
- **`assistant/`** — the player-facing application (GUI/CLI/voice). Built as the `assistant_lib` library plus the `assistant` executable; links SFML, llama.cpp, Vosk, argparse on top of `pf2e_engine`.
- **`analyzer/`** — a headless Monte-Carlo combat simulator that runs many automated battles and reports win/death probabilities. Built as `analyzer_lib` plus the `analyzer` executable; links only `pf2e_engine`.

The engine talks to the outside world solely through the `IInteractionSystem` interface (dependency-injected into `TBattle`). `assistant` supplies the human-facing `TInteractionSystem`; `analyzer` supplies the automated `TAutomatedInteractionSystem`.

## Build Commands

### Initial Setup
```bash
git clone --recursive https://github.com/stasana666/ttrpg_assistant.git
mkdir build
cd build
cmake .. -DGGML_CUDA=ON
make
```

### Building
```bash
# From build directory
cmake .. -DGGML_CUDA=ON
make

# Without CUDA (voice input will be CPU-only and impractically slow)
cmake ..
make
```

### Running Tests
```bash
# From build directory
ctest

# Run specific test suite
./pf2e_engine/tests/test_pf2_engine
./pf2e_engine/tests/actions/test_actions
./pf2e_engine/tests/actions/test_wolf_combat
./pf2e_engine/tests/ast/test_ast_state
./pf2e_engine/tests/expressions/test_expressions
./pf2e_engine/tests/game_object_logic/test_game_object_logic
./pf2e_engine/tests/inventory/test_inventory
./pf2e_engine/tests/mechanics/test_mechanics
./pf2e_engine/tests/transformation/test_transformation
```

### Running the Application
```bash
# From build directory - GUI and CLI only (no voice input)
./assistant/assistant

# With voice input support (requires Vosk and llama.cpp models)
./assistant/assistant --speech2text /path/to/vosk/model --nlp-model /path/to/llama/model.gguf
```

Model sources:
- Vosk models: https://alphacephei.com/vosk/models
- llama.cpp models: https://huggingface.co/models?apps=llama.cpp

### Running the Analyzer
```bash
# From build directory - simulate combats and print win/death probabilities
./analyzer/analyzer --iterations 1000
```

### Data Validation
```bash
# Validate all JSON game data against schema
python3 tools/data_validation.py

# Validate specific file
python3 tools/data_validation.py -t pf2e_engine/data/actions/some_action.json

# Use custom schema
python3 tools/data_validation.py -s pf2e_engine/schemas/schema.json -t pf2e_engine/data
```

## Code Architecture

### Core Game Loop
The game follows a turn-based battle system orchestrated by `TBattle` ([pf2e_engine/src/battle.cpp](pf2e_engine/src/battle.cpp)):
- **Round/Turn Management**: StartRound → StartTurn → MakeTurn → EndTurn → EndRound
- **Initiative System**: Managed by `TInitiativeOrder`
- **Action Economy**: Players receive 3 actions per turn (via `kActionId` resource), removed at turn end
- **Effect Management**: Temporary effects handled by `TEffectManager`
- **Task Scheduling**: Delayed/scheduled tasks via `TTaskScheduler`, triggered by events (`OnTurnStart`, `OnTurnEnd`)
- **Transformations**: Observable state changes through `TTransformator` with undo support
- **Savepoints / Continuations**: `TSavepointStackUnwind` suspends execution mid-turn; the `continuation::` helpers propagate it across the call stack so it can be resumed exactly where it stopped (see Continuation System below)
- **Battle End**: Battle ends when fewer than 2 teams have living players. After a battle, query `TBattle::Winner()` (the sole surviving team, or `std::nullopt`) and `TBattle::LivingTeams()`.

### Game Object System
All game entities (creatures, weapons, armor, actions, battle maps) are defined in JSON files under [pf2e_engine/data/](pf2e_engine/data/).

**Factory Pattern**: `TGameObjectFactory` ([pf2e_engine/include/pf2e_engine/game_object_logic/game_object_factory.h](pf2e_engine/include/pf2e_engine/game_object_logic/game_object_factory.h))
- Reads JSON definitions from `pf2e_engine/data/`
- Creates game objects on demand
- All JSON files are validated against [pf2e_engine/schemas/schema.json](pf2e_engine/schemas/schema.json)
- Each JSON defines its type: `pf2e_creature`, `pf2e_action`, `pf2e_weapon`, `pf2e_armor`, `pf2e_battle_map`

**Adding New Game Objects**: Create a JSON file in the appropriate subdirectory of `pf2e_engine/data/`, following the schema. The factory automatically discovers all `.json` files recursively. Always validate new JSON files with `tools/data_validation.py` before committing.

**Example JSON Files** (good references for each type):
- Creature: [warrior.json](pf2e_engine/data/creatures/warrior.json), [wizard.json](pf2e_engine/data/creatures/wizard.json)
- Weapon: [longsword.json](pf2e_engine/data/inventory/weapon/longsword.json), [dagger.json](pf2e_engine/data/inventory/weapon/dagger.json)
- Armor: [fullplate.json](pf2e_engine/data/inventory/armor/fullplate.json)
- Action: [attack.json](pf2e_engine/data/actions/attack.json), [fireball.json](pf2e_engine/data/actions/fireball.json)
- Battle map: [simpe_map.json](pf2e_engine/data/battle_maps/simpe_map.json)

### Action Pipeline System
Actions are defined as pipelines of composable blocks in JSON. The pipeline is parsed by `TActionReader` ([pf2e_engine/src/actions/action_reader.cpp](pf2e_engine/src/actions/action_reader.cpp)).

**Block Types**:
- `FunctionCall`: Execute a function with input/output parameters
- `Switch`: Branch based on success level (critical success, success, failure, critical failure)
- `ForEach`: Iterate over a list (e.g., targets in area)
- `Terminate`: End the action pipeline

**Available Functions** (in [pf2e_engine/include/pf2e_engine/action_blocks/](pf2e_engine/include/pf2e_engine/action_blocks/)):
- `choose_from_list`: Prompt the player to pick one element from a list (works on player lists, weapon lists, or DSL lists)
- `calculate_DC`: Calculate difficulty class (AC, saving throw DC)
- `roll_against_DC`: Make d20 roll against DC
- `weapon_damage_roll`: Roll weapon damage dice
- `crit_weapon_damage_roll`: Roll critical hit weapon damage
- `spell_damage_roll`: Roll spell damage (e.g., fireball 6d6)
- `deal_damage`: Apply damage to target
- `add_condition`: Apply status condition
- `remove_condition`: Remove a status condition
- `flat_damage_roll`: Roll a flat dice expression as damage (no weapon)
- `check_ally_adjacent`: Emit success if an ally is adjacent to `$target` (e.g., flanking check); pair with `Switch`
- `contribute_damage_bonus`: Add fixed bonus damage of a given type into the damage-roll accumulator
- `move`: Move character on battle map
- `get_parameter`: Extract parameter from context
- `get_targets_in_area`: Find all targets in an area (burst, cone, line). Use this for the burst/cone/line patterns that need a user-picked center cell; for emanation-shaped targeting prefer the DSL pattern (see [DSL Expression Layer](#dsl-expression-layer) below).
- `let`, `filter`, `map`, `foldl`: Generic DSL primitives (see [DSL Expression Layer](#dsl-expression-layer))

**JSON Input Syntax**: Variables are referenced with `$` prefix (e.g., `"$target"`). Damage tables map resource names to dice expressions (e.g., `{"spellslot_3": "6d6"}`). String inputs that start with `$` followed by a bare identifier (`$weapon`, `$target`) become variable references; anything else starting with `$` (like `"$item.reach >= $min_distance"`) is kept as a raw string for the DSL parser.

Actions consume resources (actions, reactions, movement) and execute their pipeline sequentially.

### DSL Expression Layer
The action pipeline embeds a small expression DSL ([pf2e_engine/{include,src}/pf2e_engine/dsl/](pf2e_engine/include/pf2e_engine/dsl/)) so that filtering, predicates, and computed values can be written inline in JSON instead of as bespoke C++ blocks. The classic example is the new [attack.json](pf2e_engine/data/actions/attack.json) pipeline: it gets all creatures, filters by line of effect, computes the minimum distance, filters weapons by reach, chooses a weapon, filters targets by that weapon's reach, and chooses a target — all expressed declaratively.

**Grammar** ([pf2e_engine/src/dsl/parser.cpp](pf2e_engine/src/dsl/parser.cpp)): variables `$name`, property access `$x.prop`, function calls `f(a, b)`, comparison `>= <= > < == !=`, logical `&& || !`, integer literals, parentheses. Arithmetic (`+ - * /`) is intentionally deferred.

**Generic blocks** (registered in [action_reader.cpp:51](pf2e_engine/src/actions/action_reader.cpp#L51)):
- `let`: evaluate a DSL `expression` and bind the result to the block's output.
- `filter`: keep elements of `list` where `predicate` (DSL with `$item` bound) is true.
- `map`: transform each element of `list` by `expression` (DSL with `$item` bound).
- `foldl`: reduce `list` via `expression` (DSL with `$acc` and `$item` bound). With optional `init`; without `init`, seeds from the list head (throws on empty). Reductions like min/max/sum are built by pairing `foldl` with a binary DSL function — no per-reduction block is needed.

**Registries** ([property_registry.h](pf2e_engine/include/pf2e_engine/dsl/property_registry.h), [function_registry.h](pf2e_engine/include/pf2e_engine/dsl/function_registry.h)): properties are registered per-class as `name → lambda(const T*, TEvalContext&) → TDslValue`; functions are stored in a single global `name → lambda(args, TEvalContext&) → TDslValue` map. Initial bindings live in [builtins.cpp](pf2e_engine/src/dsl/builtins.cpp):
- Properties: `TWeapon.reach`, `TPlayer.creature`, `TPlayer.weapons`.
- Functions: `creatures()`, `distance(a, b)`, `has_line_of_effect(a, b)`, `min(a, b)`, `max(a, b)`.

**Value type**: [`TDslValue`](pf2e_engine/include/pf2e_engine/dsl/value.h) is a separate variant from `TGameObjectPtr`, kept separate so adding bool/list alternatives doesn't ripple into every `std::visit` site on the registry. Conversion happens at the pipeline boundary via [value_convert.h](pf2e_engine/include/pf2e_engine/dsl/value_convert.h). When extending: prefer adding properties/functions over adding new block types; the existing four collection blocks cover almost everything once a binary reducer is registered.

**Scope binding**: `filter`/`map`/`foldl` bind `$item` (and `$acc` for foldl) into a scoped variable map on `TEvalContext` via [`TScopeGuard`](pf2e_engine/include/pf2e_engine/dsl/expression.h). The scope shadows the registry, so nested filters/maps with the same name correctly nest and restore.

**`TDslValue(bool)` is constrained to `std::same_as<B, bool>`** ([value.h](pf2e_engine/include/pf2e_engine/dsl/value.h)) — without it, a `const T*` (when no matching non-const pointer overload exists) silently coerces to `bool` via implicit pointer-to-bool conversion and the variant ends up with the wrong alternative. If you add a new pointer alternative, prefer non-const access at the call site rather than introducing const-pointer constructors.

**Examples in data**: [attack.json](pf2e_engine/data/actions/attack.json), [demoralize.json](pf2e_engine/data/actions/demoralize.json), [remove_fear.json](pf2e_engine/data/actions/remove_fear.json), [trip.json](pf2e_engine/data/actions/trip.json), [attacks_of_opportunity.json](pf2e_engine/data/actions/attacks_of_opportunity.json) all use the DSL for targeting. [fireball.json](pf2e_engine/data/actions/fireball.json) and [burst_of_fear.json](pf2e_engine/data/actions/burst_of_fear.json) still use `get_targets_in_area` because the burst pattern needs the engine to prompt for a center cell — adding that to the DSL would require a `TPosition` value type and a `choose_position` function.

**Tests**: [pf2e_engine/tests/dsl/](pf2e_engine/tests/dsl/) covers the lexer, parser, evaluator, property access, function calls, and scope nesting.

### Interaction System
The engine asks the outside world to make choices through the `IInteractionSystem` interface ([pf2e_engine/include/pf2e_engine/i_interaction_system.h](pf2e_engine/include/pf2e_engine/i_interaction_system.h)). It is dependency-injected into `TBattle` as `IInteractionSystem&`, so the engine never depends on any concrete implementation.
- Choices are presented via `TAlternatives` with kind labels (e.g., "next action", "target", "burst center")
- `IInteractionSystem::ChooseAlternative<T>()` short-circuits when only one alternative exists
- `IInteractionSystem::HandleReactionTrigger()` reports a reaction opportunity (e.g. `OnMove`); the implementation alone decides whether to resolve it immediately (return) or defer it by throwing `TSavepointStackUnwind` — the engine never makes this decision

Concrete implementations:
- **`TInteractionSystem`** ([assistant/src/interaction_system.cpp](assistant/src/interaction_system.cpp)) — the player-facing implementation. Supports multiple simultaneous input sources (GUI, CLI, voice), uses channel-based communication (`TChannel<TClickEvent>`), runs the GUI in the main thread and game logic in a separate thread. Voice input (`TAudioInputSystem`) uses Vosk for speech-to-text and llama.cpp for intent recognition. **Asking Strategies**: `EAskingStrategy::Console` for CLI/voice, `EAskingStrategy::Gui` for click-based input.
- **`TAutomatedInteractionSystem`** ([analyzer/include/analyzer/automated_interaction_system.h](analyzer/include/analyzer/automated_interaction_system.h)) — the headless simulation implementation. Delegates every choice to an `IDecisionStrategy` and discards all log output.
- **`TMockInteractionSystem`** ([pf2e_engine/tests/test_lib/mock_interaction_system.h](pf2e_engine/tests/test_lib/mock_interaction_system.h)) — the scripted test implementation.

### Analyzer
The combat analyzer ([analyzer/](analyzer/)) runs Monte-Carlo simulations on top of the headless engine:
- **`IDecisionStrategy`** ([analyzer/include/analyzer/decision_strategy.h](analyzer/include/analyzer/decision_strategy.h)) — pluggable policy that picks a choice index from `TAlternatives` by inspecting its `Kind()`. New strategies (defensive, random, etc.) are added by implementing this interface.
- **`TAggressiveMeleeStrategy`** ([analyzer/src/aggressive_melee_strategy.cpp](analyzer/src/aggressive_melee_strategy.cpp)) — the first concrete strategy: always picks the weapon-attack action and an enemy target.
- **`TCombatAnalyzer`** ([analyzer/src/combat_analyzer.cpp](analyzer/src/combat_analyzer.cpp)) — builds the `TGameObjectFactory` once, then loops fresh `TBattle` + seeded `TRandomGenerator` per run, aggregating per-team win rate and per-creature death probability into `TAnalysisResult`.

### Expression System
Mathematical expressions (damage rolls, stat calculations) use a compositional expression tree, all implementing `IExpression`:
- `TDiceExpression`: Single dice roll (e.g., "2d6")
- `TMultiDiceExpression`: Sum of several `TDiceExpression`s
- `TNumberExpression`: Constant value
- `TSumExpression`: Addition / subtraction of sub-expressions
- `TProductExpression`: Multiplication / division of sub-expressions
- Defined in [pf2e_engine/include/pf2e_engine/expressions/](pf2e_engine/include/pf2e_engine/expressions/)

### Transformation System
State changes use a command pattern for reversibility. `TTransformator` ([pf2e_engine/src/transformation/transformator.cpp](pf2e_engine/src/transformation/transformator.cpp)) is a variant-based command queue; `Undo(state)` rolls back to a previously-captured `TState`. Each variant stores the prior value it needs to invert itself (defined in [transformation.h](pf2e_engine/include/pf2e_engine/transformation/transformation.h)):
- `TChangeHitPoints` — modify HP
- `TChangeCondition` — set a creature condition value
- `TChangeResource` — add or reduce a resource (actions, reactions, movement, spellslots)
- `TAddEffect` / `TRemoveEffect` — install or uninstall a timed effect on a player
- `TAddTask` / `TRemoveTask` — schedule or unschedule a `TTask` in `TTaskScheduler`
- `TAdvanceTaskProgress` — advance a task's event-cursor
- `TChangeCurrentPlayer` / `TChangeRound` — initiative-order updates

Any mutation that skips `TTransformator` will survive rollback — see the AST-Based State Comparison section below for the test that catches that.

### AST-Based State Comparison
Validates save/rollback correctness: `TTransformator::Undo` must restore **all** engine state. The AST system serializes a `TBattle` into a deterministic tree (`TAstNode GetAst(TAstContext&) const` on each instrumented class) so tests can assert byte-equal trees before vs. after a mutate/rollback cycle. Any mutation that bypassed `TTransformator` shows up as a diff at a named path.

Layout-change detection: every instrumented class ends with a trailing `ast_layout_sentinel_` member and `GetAst` opens with `AST_ASSERT_LAYOUT[_WITH_SENTINEL]`. Adding a new field forces a compile failure, so the contributor must audit `GetAst` and decide the field's ownership category.

- Tests: [pf2e_engine/tests/ast/test_ast_state.cpp](pf2e_engine/tests/ast/test_ast_state.cpp)
- Design doc: [pf2e_engine/include/pf2e_engine/common/ast/CLAUDE.md](pf2e_engine/include/pf2e_engine/common/ast/CLAUDE.md) — read this before adding `GetAst` to a new class or changing the AST infrastructure.

### Continuation / Savepoint System
Some interactions must be deferred without halting the game (e.g. a reaction opportunity raised when a creature moves). `TSavepointStackUnwind` ([pf2e_engine/include/pf2e_engine/actions/save_point.h](pf2e_engine/include/pf2e_engine/actions/save_point.h)) is an exception thrown to *suspend* execution: as it unwinds, each continuation-aware frame appends "the rest of its work" via `AddCallFunctionLevel`; `TBattle::MakeTurn` catches it, and `Resume()` re-enters every frame at its suspension point.

The `continuation::` helpers ([pf2e_engine/include/pf2e_engine/common/continuation.h](pf2e_engine/include/pf2e_engine/common/continuation.h)) centralize that propagation so call sites never hand-write the `try/catch(TSavepointStackUnwind)/rethrow` pattern:
- `continuation::Then(step, tail)` — run `step` then `tail`; `tail` survives a suspension inside `step`
- `continuation::While(condition, body)` — continuation-aware loop; `TAction::Apply` uses it to drive the block pipeline
- `continuation::ForEach` / `ForEachOwned` — continuation-aware iteration; `TForEachBlock` uses them

Whether to suspend at all is decided solely by `IInteractionSystem::HandleReactionTrigger` — the analyzer resolves reactions immediately, while the assistant defers them so the game flow is not blocked.

### Testing Framework
Tests use GoogleTest with custom mocks ([pf2e_engine/tests/test_lib/](pf2e_engine/tests/test_lib/)):

**TMockRng**: Deterministic dice roller for reproducible tests
```cpp
mock_rng_.ExpectCall(20, 10);  // Expect d20 roll, return 10
mock_rng_.Verify();            // Assert all expected calls were made
```

**TMockInteractionSystem**: Simulates player choices
```cpp
mock_interaction_.ExpectChoice(player_id, "next action", "attack_with_weapon");
mock_interaction_.ExpectChoice(player_id, "target", "Warrior 2");
mock_interaction_.AddCheckCallback([&]() {
    // Assert game state mid-test
    EXPECT_EQ(player->GetCreature()->Hitpoints().GetCurrentHp(), 12);
});
mock_interaction_.Verify();
```

When mocks run out of expected calls, `TTooManyCallsError` is thrown. See [test_action_combat.cpp](pf2e_engine/tests/actions/test_action_combat.cpp) for examples of combat tests including attacks, misses, crits, and Multiple Attack Penalty (MAP).

### Key Dependencies
- **nlohmann/json**: JSON parsing
- **json-schema-validator**: Validating game data
- **GoogleTest**: Unit testing framework
- **SFML**: Graphics/GUI
- **llama.cpp**: LLM inference (optional, for voice)
- **Vosk**: Speech-to-text (optional, for voice)
- **argparse**: Command-line argument parsing

### Vosk Configuration
The path to `libvosk.so` is configured in [extern/CMakeLists.txt](extern/CMakeLists.txt):8. Default location assumes Python venv installation. Update `VOSK_LIB` variable if Vosk is installed elsewhere.

### Important File Locations
- Engine source: [pf2e_engine/src/](pf2e_engine/src/) and [pf2e_engine/include/pf2e_engine/](pf2e_engine/include/pf2e_engine/)
- AST state-comparison infrastructure: [pf2e_engine/include/pf2e_engine/common/ast/](pf2e_engine/include/pf2e_engine/common/ast/) (has its own CLAUDE.md)
- Engine tests: [pf2e_engine/tests/](pf2e_engine/tests/)
- Game data: [pf2e_engine/data/](pf2e_engine/data/)
- JSON schemas: [pf2e_engine/schemas/](pf2e_engine/schemas/)
- Assistant app (GUI/CLI/voice): [assistant/src/](assistant/src/), [assistant/include/assistant/](assistant/include/assistant/)
- Assistant entry point: [assistant/main/main.cpp](assistant/main/main.cpp)
- Images: [assistant/images/](assistant/images/)
- Analyzer: [analyzer/src/](analyzer/src/), [analyzer/include/analyzer/](analyzer/include/analyzer/)
- Analyzer entry point: [analyzer/main/analyzer_main.cpp](analyzer/main/analyzer_main.cpp)

Each component has its own CMakeLists.txt; the top-level [CMakeLists.txt](CMakeLists.txt) adds `extern`, then `pf2e_engine`, `assistant`, `analyzer`. Game data and schemas stay under `pf2e_engine/`; `kRootDirPath` (from generated `cpp_config.h`) resolves data via `kRootDirPath + "/pf2e_engine/data"`.

## Compilation Settings
- C++ Standard: C++23 (required)
- Compiler flags: `-Wall -Wextra -Wpedantic -Werror -g`
- clang-tidy is enabled for static analysis

## Naming Conventions
- **Types**: `T` prefix (e.g., `TBattle`, `TWeapon`, `TAction`)
- **Enums**: `E` prefix (e.g., `ECondition`, `ESuccessLevel`, `EProficiencyLevel`)
- **Interfaces**: `I` prefix for abstract base classes (e.g., `IActionBlock`, `IExpression`, `IInteractionSystem`)
- **Functions**: `F` prefix for callable action block functions (e.g., `FAddCondition`, `FFilter`)
- **Constants**: `k` prefix with PascalCase (e.g., `kEmptyBlockFactory`, `kFunctionMapping`)
- **ID Types**: `T{Name}Id` with `T{Name}IdManager` singleton (e.g., `TGameObjectId`, `TResourceId`)

## How to Extend the Action Language

**Prefer the DSL first.** Most "new block" needs really mean "expose a new property or function so the existing `let`/`filter`/`map`/`foldl` can do the job in JSON":
- New property (e.g. `$creature.hp`): add an entry to the appropriate `TPropertyRegistry<T>::Instance().Register(...)` in [builtins.cpp](pf2e_engine/src/dsl/builtins.cpp).
- New function (e.g. `is_ally(a, b)`): add an entry to `TDslFunctionRegistry::Instance().Register(...)` in the same file.
No new C++ class, no `kFunctionMapping` change. The existing combat tests will exercise it automatically once an action JSON uses it.

**Add a new action block only when:**
- The work has irreducible C++ side effects on engine state — e.g. rolling dice via the RNG, recording a transformation, scheduling a task, prompting the interaction system. These can't be expressed as pure DSL expressions.
- The new operation needs to write a NAMED output back into the registry beyond what `let` covers.

Steps to add a new block:
1. **Create header** in `pf2e_engine/include/pf2e_engine/action_blocks/`:
   ```cpp
   class FYourBlock : public FBaseFunction {
   public:
       FYourBlock(TBlockInput&& input, TGameObjectId output)
           : FBaseFunction(std::move(input), output) {}
       void operator()(std::shared_ptr<TActionContext> ctx) const;
   };
   ```

2. **Create implementation** in `pf2e_engine/src/action_blocks/your_block.cpp`

3. **Register** in [action_reader.cpp](pf2e_engine/src/actions/action_reader.cpp):
   - Add include for your header
   - Add entry to `TPipelineReader::kFunctionMapping`

4. **Update CMakeLists.txt**: Add `.cpp` to `target_sources()` in `pf2e_engine/src/action_blocks/CMakeLists.txt`

5. **Use in JSON**: `"function": "your_block"` in action pipeline

## Key Data Structures
- **TCreature**: Core entity with stats, proficiency, weapons, armor, resources, conditions
- **TPlayer**: Wrapper around TCreature with team, ID, name, position binding
- **TResourcePool**: Tracks action/reaction/movement costs via `TResourceId`
- **TBlockInput**: Action block parameters as `std::variant<string, int, TGameObjectId, TDamageTable>`
- **TAlternatives**: Choices presented to player with kind label and typed values

## Common Patterns
- **ID Registry**: `TValueIdManager<Tag>::Instance().Register("name")` for string-to-ID mapping with O(1) lookup
- **Observable**: `TObservable<Context>` for publish-subscribe state changes
- **Channel**: `TChannel<T>` for lock-free inter-thread communication (GUI ↔ game logic)
- **Variant Types**: Heavy use of `std::variant` for polymorphic values
- **Continuation**: `continuation::While`/`ForEach`/`Then` ([continuation.h](pf2e_engine/include/pf2e_engine/common/continuation.h)) propagate `TSavepointStackUnwind` suspensions across the call stack

## Known Limitations
- Inline armor/weapon definitions in creature JSON not yet supported
- Interaction system uses spinlock polling (TODO: condition_variable)
- DSL has no arithmetic operators (`+ - * /`) yet — comparison and logical only
- Burst/cone/line AoE patterns still go through `get_targets_in_area` because the DSL has no `TPosition` value type or position-picking function — would be a small extension if needed

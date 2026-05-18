# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a tabletop RPG (TTRPG) assistant for D&D/Pathfinder (PF2e), written in C++23. It provides a game engine for managing combat encounters with support for multiple interfaces: GUI (SFML), CLI, and optional voice input (Vosk + llama.cpp).

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
./pf2e_engine/tests/expressions/test_expressions
./pf2e_engine/tests/game_object_logic/test_game_object_logic
./pf2e_engine/tests/inventory/test_inventory
./pf2e_engine/tests/mechanics/test_mechanics
```

### Running the Application
```bash
# From build directory - GUI and CLI only (no voice input)
./pf2e_engine/main/pf2e_engine

# With voice input support (requires Vosk and llama.cpp models)
./pf2e_engine/main/pf2e_engine --speech2text /path/to/vosk/model --nlp-model /path/to/llama/model.gguf
```

Model sources:
- Vosk models: https://alphacephei.com/vosk/models
- llama.cpp models: https://huggingface.co/models?apps=llama.cpp

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
- **Savepoints**: `TSavepointStackUnwind` allows reverting actions mid-turn (e.g., for "undo" functionality)
- **Battle End**: Battle ends when fewer than 2 teams have living players

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
- `choose_weapon`: Select weapon for attack
- `choose_from_list`: Present choices to player
- `calculate_DC`: Calculate difficulty class (AC, saving throw DC)
- `roll_against_DC`: Make d20 roll against DC
- `weapon_damage_roll`: Roll weapon damage dice
- `crit_weapon_damage_roll`: Roll critical hit weapon damage
- `spell_damage_roll`: Roll spell damage (e.g., fireball 6d6)
- `deal_damage`: Apply damage to target
- `add_condition`: Apply status condition
- `move`: Move character on battle map
- `get_parameter`: Extract parameter from context
- `get_targets_in_area`: Find all targets in an area (burst, cone, etc.)

**JSON Input Syntax**: Variables are referenced with `$` prefix (e.g., `"$target"`). Damage tables map resource names to dice expressions (e.g., `{"spellslot_3": "6d6"}`).

Actions consume resources (actions, reactions, movement) and execute their pipeline sequentially.

### Interaction System
`TInteractionSystem` ([pf2e_engine/src/interaction_system.cpp](pf2e_engine/src/interaction_system.cpp)) provides an abstraction layer for user interaction:
- Supports multiple simultaneous input sources (GUI, CLI, voice)
- Uses channel-based communication (`TChannel<TClickEvent>`)
- GUI runs in main thread, game logic runs in separate thread
- Voice input system (`TAudioInputSystem`) uses Vosk for speech-to-text and llama.cpp for intent recognition
- **Asking Strategies**: `EAskingStrategy::Console` for CLI/voice, `EAskingStrategy::Gui` for click-based input
- Choices are presented via `TAlternatives` with kind labels (e.g., "next action", "target", "burst center")

### Expression System
Mathematical expressions (damage rolls, stat calculations) use a compositional expression tree:
- `TDiceExpression`: Dice rolls (e.g., "2d6")
- `TNumberExpression`: Constant values
- `TMathExpression`: Arithmetic operations (+, -, *, /)
- Defined in [pf2e_engine/include/pf2e_engine/expressions/](pf2e_engine/include/pf2e_engine/expressions/)

### Transformation System
State changes use a command pattern for reversibility ([pf2e_engine/src/transformation/transformation.cpp](pf2e_engine/src/transformation/transformation.cpp)):
- `TChangeHitPoints`: Modify HP with stored previous state for undo
- `TTransformator`: Manages transformation queue and rollback

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
- Source code: [pf2e_engine/src/](pf2e_engine/src/) and [pf2e_engine/include/pf2e_engine/](pf2e_engine/include/pf2e_engine/)
- Entry point: [pf2e_engine/main/main.cpp](pf2e_engine/main/main.cpp)
- Tests: [pf2e_engine/tests/](pf2e_engine/tests/)
- Game data: [pf2e_engine/data/](pf2e_engine/data/)
- JSON schemas: [pf2e_engine/schemas/](pf2e_engine/schemas/)
- Images: [pf2e_engine/images/](pf2e_engine/images/)

## Compilation Settings
- C++ Standard: C++23 (required)
- Compiler flags: `-Wall -Wextra -Wpedantic -Werror -g`
- clang-tidy is enabled for static analysis

## Naming Conventions
- **Types**: `T` prefix (e.g., `TBattle`, `TWeapon`, `TAction`)
- **Enums**: `E` prefix (e.g., `ECondition`, `ESuccessLevel`, `EProficiencyLevel`)
- **Interfaces**: `I` prefix for abstract base classes (e.g., `IActionBlock`, `IExpression`, `IInteractionSystem`)
- **Functions**: `F` prefix for callable action block functions (e.g., `FChooseWeapon`, `FAddCondition`)
- **Constants**: `k` prefix with PascalCase (e.g., `kEmptyBlockFactory`, `kFunctionMapping`)
- **ID Types**: `T{Name}Id` with `T{Name}IdManager` singleton (e.g., `TGameObjectId`, `TResourceId`)

## How to Add New Action Blocks
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

## Known Limitations
- Weapon selection always chooses first in slots (hardcoded in `choose_weapon.cpp`)
- Inline armor/weapon definitions in creature JSON not yet supported
- Interaction system uses spinlock polling (TODO: condition_variable)

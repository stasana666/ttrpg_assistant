# PF2E Engine

Canonical project instructions. CLAUDE.md redirects here.

## Purpose
- Core headless PF2E game-mechanics engine: battle state, maps, players, resources, actions, mechanics, inventory, transformations, continuations, and AST state comparison.
- Public headers live in `include/pf2e_engine/`; implementations live in `src/`; data lives in `data/`; tests live in `tests/`.
- Keep this code independent of assistant GUI/audio and analyzer strategy implementations.

## Key Concepts
- `TBattle` orchestrates rounds, turns, initiative, resources, transformations, action history, the encounter clock, relations and battle end state.
- The engine talks to outside systems only through `IInteractionSystem`; assistant, analyzer, and tests provide concrete implementations.
- `CreateAbilityRules()` (`rules.h`) builds the immutable `TAbilityRules` that every battle shares. Creatures, items and map layouts come from generated `.ttrpg` constructors.
- Creature, inventory and condition types and most PF2e rules are generated from `.ttrpg` schemas in `data/schemas/`; `TCreature` is a generated `GameEntity` with no handwritten subclass.
- Abilities come from grants on current creature, race, class and weapon features. `TBattle::AvailableActions` and `Execute(option)` enumerate and run them; typed reactions run through reaction windows opened by compiled rules.
- Every action, reaction and lifecycle hook is a compiled `.ttrpg` body. `TAction` also accepts a hand-written `FBody`, which runtime tests use.
- Rollback and resume depend on `TTransformator`, `TSavepointStackUnwind`, continuation helpers and AST state comparison. Runtime invariants are documented in `include/pf2e_engine/common/AGENTS.md`.

## Build/Test
- Build engine: `cmake --build build --target pf2e_engine`.
- Broad check: `ctest --test-dir build --output-on-failure`; see `tests/AGENTS.md` for targets and shared fixtures.
- Headless encounter: `./build/analyzer/analyzer --scenario fighter-wolves --iterations 10`.

## Extending Actions
- Write new rules as compiled `.ttrpg` bodies. Extend the compiler through small generic runtime adapters and keep PF2e formulas in rule declarations.

## Pitfalls
- Do not edit generated headers/sources in `build/generated`; edit schemas or codegen instead.
- Mutations that bypass `TTransformator` can survive rollback and should be caught by AST tests.
- Use local docs under `data/`, `data/schemas/`, `include/pf2e_engine/common/`, and `tests/` before loading unrelated engine context.

# Analyzer

## Purpose
- Automated combat simulation harness using `pf2e_engine`, separate from the interactive assistant.
- Current implementation runs repeated two-warrior battles with deterministic RNG seeds and an automated interaction system.

## Key Files
- `include/analyzer/decision_strategy.h`: strategy interface for choosing among `TAlternatives`.
- `aggressive_melee_strategy.*`: simple strategy preferring attacks and enemy targets.
- `automated_interaction_system.h`: `IInteractionSystem` implementation that suppresses logs and delegates choices.
- `combat_analyzer.*`: loads engine data and aggregates wins/deaths/draws.
- `main/analyzer_main.cpp`: CLI entry point with `--iterations`.

## Build/Test
- Build: `cmake --build build --target analyzer`.
- Run: `./build/analyzer/analyzer --iterations 1000`.
- No dedicated analyzer tests exist; verify engine behavior with relevant `pf2e_engine/tests` targets.

## Pitfalls
- Keep analyzer strategies independent of assistant UI/audio code.
- Do not change engine combat rules just to make an analysis result look different; change strategy or test setup unless explicitly asked.
- The analyzer loads real `pf2e_engine/data`, so data/schema changes affect simulations.

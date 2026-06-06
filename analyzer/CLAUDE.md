# analyzer — headless Monte-Carlo combat simulator

Runs many automated battles on top of the headless engine and reports win/death
probabilities (`analyzer_lib` library + `analyzer` executable; links only
`pf2e_engine`). It supplies the automated `IInteractionSystem` — see the root
[CLAUDE.md](../CLAUDE.md) "Interaction System" section for the engine-side
contract. Keep analyzer code independent of assistant GUI/audio.

## Layout

- [include/analyzer/decision_strategy.h](include/analyzer/decision_strategy.h) —
  `IDecisionStrategy`: pluggable policy that picks a choice index from
  `TAlternatives` by inspecting its `Kind()`. Add new strategies (defensive,
  random, …) by implementing this interface.
- [src/aggressive_melee_strategy.cpp](src/aggressive_melee_strategy.cpp) —
  `TAggressiveMeleeStrategy`: always picks the weapon-attack action and an enemy
  target.
- [include/analyzer/automated_interaction_system.h](include/analyzer/automated_interaction_system.h) —
  `TAutomatedInteractionSystem`: delegates every choice to an `IDecisionStrategy`,
  discards log output, and **resolves reaction triggers immediately** (unlike the
  assistant, which defers them).
- [src/combat_analyzer.cpp](src/combat_analyzer.cpp) — `TCombatAnalyzer`: builds
  the `TGameObjectFactory` once, then loops a fresh `TBattle` + seeded
  `TRandomGenerator` per run, aggregating per-team win rate and per-creature death
  probability into `TAnalysisResult`.
- [main/analyzer_main.cpp](main/analyzer_main.cpp) — CLI entry point (`--iterations`).

## Build / run

- Build: `cmake --build build --target analyzer`.
- Run: `./build/analyzer/analyzer --iterations 1000`.
- No dedicated analyzer tests; verify combat behavior via the relevant
  `pf2e_engine/tests` targets.

## Pitfalls

- The analyzer loads the real [pf2e_engine/data](../pf2e_engine/data) tree, so
  data/schema changes affect simulation results.
- Do not change engine combat rules to move an analysis number — adjust the
  strategy or test setup instead unless explicitly asked.

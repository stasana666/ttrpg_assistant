# analyzer — headless Monte-Carlo combat simulator

Canonical project instructions. CLAUDE.md redirects here.

Runs many automated battles on top of the headless engine and reports win/death
probabilities (`analyzer_lib` library + `analyzer` executable; links only
`pf2e_engine`). It supplies the automated `IInteractionSystem` — see the engine
[AGENTS.md](../pf2e_engine/AGENTS.md) "Key Concepts" section for the engine-side
contract. Keep analyzer code independent of assistant GUI/audio.

## Layout

- [include/analyzer/decision_strategy.h](include/analyzer/decision_strategy.h) —
  `IDecisionStrategy`: pluggable policy that picks a choice index from
  `TAlternatives` by inspecting its `Kind()`. Add new strategies (defensive,
  random, …) by implementing this interface.
- [src/aggressive_melee_strategy.cpp](src/aggressive_melee_strategy.cpp) —
  `TAggressiveMeleeStrategy`: selects melee attacks, movement, defensive actions
  and enemy targets using the current battle.
- [include/analyzer/automated_interaction_system.h](include/analyzer/automated_interaction_system.h) —
  `TAutomatedInteractionSystem`: delegates every choice to an `IDecisionStrategy`,
  discards log output. Generated
  reaction windows request ordinary choices through the decision strategy.
- [src/combat_analyzer.cpp](src/combat_analyzer.cpp) — `TCombatAnalyzer`: builds
  the ability rules once, then loops a fresh `TBattle` + seeded
  `TRandomGenerator` per run, aggregating per-team win rate and per-creature death
  probability into `TAnalysisResult`.
- [main/analyzer_main.cpp](main/analyzer_main.cpp) — CLI entry point (`--iterations`).

## Build / run

- Build: `cmake --build build --target analyzer`.
- Run: `./build/analyzer/analyzer --iterations 1000`.
- `test_analyzer_strategy` checks declared melee reach, source-specific enemy
  targeting, stale choices, and conservative reaction selection. Engine rule
  behavior remains covered by the relevant `pf2e_engine/tests` targets.

## Pitfalls

- Combatants come from the generated Remaster constructors, so schema changes
  affect simulation results.
- Do not change engine combat rules to move an analysis number — adjust the
  strategy or test setup instead unless explicitly asked.
- Action alternatives carry `optional<TAbilityOption>`; the empty value ends a turn. The aggressive strategy recognizes `abilities.UseStrike` and `abilities.Strike` by definition ID, independently of display labels and the particular weapon source.

## Generated Fighter and Wolves
- `./build/analyzer/analyzer --scenario fighter-wolves --iterations 100` runs one generated `HumanFighter1` against two generated `Wolf` values; `RunFighterAgainstWolves` accepts the same options as the generated factory. The default scenario `two-fighters` runs two generated `HumanFighter1` values against each other. Both scenarios share one encounter loop on a 12 by 12 map.
- Bind `TAggressiveMeleeStrategy` to its battle before multi-team encounters. It targets the weakest conscious enemy, stands when prone, uses Knockdown, raises a shield with its last action, prefers aggregate `UseStrike`, and otherwise closes distance with Stride or Sudden Charge using `TCombatGeometry.InReach`.
- It accepts Shield Block, and Reactive Strike only for a lone team member, because reaction alternatives do not name the triggering creature. Other reactions are declined. This is a simple deterministic policy, not optimal play.
- Encounter reports keep one entry per creature: allies share their team's wins, and death counts use each creature's `IsDead`.

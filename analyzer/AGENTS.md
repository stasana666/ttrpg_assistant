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
  discards log output, and ignores legacy reaction notifications. Generated
  reaction windows request ordinary choices through the decision strategy.
- [src/combat_analyzer.cpp](src/combat_analyzer.cpp) — `TCombatAnalyzer`: builds
  the `TGameObjectFactory` once, then loops a fresh `TBattle` + seeded
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

- The analyzer loads the real [pf2e_engine/data](../pf2e_engine/data) tree, so
  data/schema changes affect simulation results.
- Do not change engine combat rules to move an analysis number — adjust the
  strategy or test setup instead unless explicitly asked.
- Action alternatives carry `optional<TAbilityOption>`; the empty value ends a turn. The aggressive strategy recognizes `abilities.AttackWithWeapon` and `abilities.Strike` by definition ID, independently of display labels and the particular weapon source.

## Generated Fighter and Wolves
- `./build/analyzer/analyzer --scenario fighter-wolves --iterations 100` runs one
  generated `HumanFighter1` against two independent generated `Wolf` values on an
  open grid. `RunFighterAgainstWolves(iterations, THumanFighter1Options)` also accepts
  the same configurable weapon/armor/shield/feat options as the generated factory.
  The default CLI scenario remains `two-warriors` for historical JSON comparison.
- Bind `TAggressiveMeleeStrategy` to its battle before running multi-team encounters.
  It selects conscious enemies by team, understands generic UseStrike and source
  choices, stands when prone, uses available Knockdown, raises an unraised shield
  with the final action, and closes distance through Stride or Sudden Charge.
  Typed offers accept Shield Block and Reactive Strike for a lone team member.
  With allies present, the policy declines Reactive Strike because generic reaction
  alternatives currently omit the triggering subject; it cannot safely identify
  that subject as an enemy. Other reaction definitions default to decline. This is a simple
  deterministic tactical policy, not optimal-play evidence; it does not evaluate
  Demoralize, weapon damage-type defenses or every feat strategy.
- Attack and movement-stop decisions use generated `TCombatGeometry.InReach`
  and the world's line-of-effect policy. Source-bound Strike checks that its
  particular weapon reaches a conscious enemy; aggregate UseStrike takes
  precedence when available. Without an applicable attack the policy tries
  movement, and without conscious enemies it ends the turn. Position scoring
  remains a local distance heuristic, without obstacle pathfinding.
- Generated encounter reports retain an entry per creature; both wolves receive
  their team's win result, while each death counter uses that creature's IsDead.
  An unconscious surviving fighter can lose without dying. Historical two-warrior
  reporting retains its earlier legacy IsAlive semantics.

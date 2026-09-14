# Engine Tests

Canonical project instructions. CLAUDE.md redirects here.

## Purpose
- GoogleTest coverage for engine mechanics, actions, runtime DSL, inventory data, game object loading, AST snapshots, and transformations.

## Key Files
- `test_knockout_initiative.cpp` follows generated and legacy damage delivery through
  an actual battle. It checks that a new knockout delays recovery until the next
  initiative cycle, preserves the current actor, and leaves death/legacy defeat
  ordering unchanged. Full battle and journal snapshots survive Undo and replay.
- `test_lib/mock_dice_roller.h`: deterministic RNG expectations for reproducible tests.
- `test_lib/mock_interaction_system.h`: scripted player choices and optional state checks.
- `test_lifecycle.cpp` builds as `test_lifecycle` and verifies generated round/turn
  methods, separate Flourish and reaction reset boundaries, initial reaction policy,
  scheduler ordering, resource validation and exact JSON/AST rollback. Task removal
  and repeated Undo preserve callback order; malformed task restoration leaves the
  scheduler unchanged. Battle tests cover three independent participants and
  restoration across a whole round.
- `test_lifecycle_execution.cpp` and `actions/fixtures/generated_lifecycle.ttrpg`
  verify system phases with inactive actors, two successive suspensions, copied
  service lifetimes, exact Undo/replay without rerolls and callback protocol errors.
  Ordinary actions cannot retain lifecycle privilege; suspended legacy bodies stop
  after unconsciousness or death. Factory/battle integration runs recovery from
  transitive Toughness grants and skips unconscious actors' ordinary action prompts.
- `test_initiative.cpp` builds as `test_initiative` and checks generated Perception
  initiative, selected skills and typed modifiers, descending rolled totals,
  explicit tie order and stable insertion. It covers natural 1/20 ranking,
  independent NPC totals, current-stat changes without rerolling stored order,
  round replay, exact journal rollback and failed battle insertion. Native tie
  ordering uses encounter options without inferring PC/NPC roles from teams.
  Relocation cases move already-acted, unacted and current entries without rerolls,
  preserve the active participant and restore previous successors through multiple
  moves and round advancement. Runtime tests distinguish the current turn from a
  reacting actor, validate writable membership even during setup, and reject late
  or duplicate enrollment before RNG or map changes.
- `test_health.cpp` builds as `test_health` and verifies explicit defeat/dying/death
  policies, normal and critical knockout, nonlethal damage, temporary HP, defenses,
  massive damage and four-degree recovery with explicit DC adjustments. Healing,
  stabilization, wounded and native adapters share exact journal rollback and
  replay. Failed nested transitions undo their HP writes; JSON import and snapshots
  preserve health state. Unconscious creatures regain turn resources but cannot
  use them, while dead creatures do not regain actions or reactions.
- `actions/`: combat/action pipeline tests using real data.
- `actions/test_time_relations.cpp` builds as `test_time_relations` and tests ordered
  entity pairs, false and ordinary-class payloads, readonly capabilities, exact
  deadlines, permanent entries, overflow and expired-state purity. It verifies
  context release, source death and feature removal, registry failures, normalized
  AST equality, new-encounter boundaries and suspended write replay. A real
  three-participant round advances the authored clock once and restores both time
  and relations through initiative rollback; partial-round victory leaves time
  unchanged. No scheduler is involved.
- `actions/fixtures/generated_relations.ttrpg` and `actions/test_generated_relations.cpp`
  build as `test_generated_relations`. They execute typed class, Position, string,
  enum and false-valued relation payloads through generated actions. Two suspensions
  retain the original value and duration; stored lookup copies survive replacement.
  Generated pure hooks read current pairs and expiration without journal writes.
  A selected-source child activity reads relations under its input validators, and
  a yielded reaction writes the reactor's pair before resuming the parent. Exact
  snapshots and Undo cover the combined costs, relation state and continuations.
- `actions/test_action_history.cpp` builds as `test_action_history`. It checks
  detached typed results, readonly enumeration and mutable prepared inputs,
  payment rejection versus accepted body failure, source-owner validation,
  immutable version stamps across divergent rollback branches, stale parent
  publication, sparse source restoration and context release. Receipt and journal
  AST snapshots include semantic payloads and identity validity while normalizing
  independent entity IDs. Nested activity inputs keep validating consumed-record
  sources without recursive context validation, including removal and restoration.
  Turn methods do not implicitly expire receipts.
- `actions/fixtures/generated_records.ttrpg` and `actions/test_generated_records.cpp`
  build as `test_generated_records`. They check terminal action publication after
  yielded child activities, exact target/source history, selected source aliases,
  consumed follow-up inputs and stale option versions. Own accepted actions and
  reactions invalidate prior results; other actors and rejected preflight do not.
  Historical sources retain their value snapshot independently of the old grant,
  while replacement or removal invalidates their identity. Two-stage suspension
  and journal replay preserve both history and effects. The fixture uses generic
  actions, not the production PF2e Knockdown rule.
- `actions/test_generated_shield.cpp` builds as `test_generated_shield` and executes
  Raise a Shield and Shield Block against production compiled Strike. It checks
  circumstance bonus merging, resistance before Hardness, temporary HP after the
  block, breaking/destruction, decline, physical damage eligibility and knockout
  after reaction completion. Suspended offers recheck the raised shield before
  payment and replay the pending damage without rerolls. Nested shield values
  round-trip through JSON/AST; invalid blocks preserve the journal. Turn-start
  expiration lowers only the current actor's shield before scheduled effects and
  resource replenishment. Grip and legacy mixed-damage packets remain separate.
- `actions/test_generated_movement.cpp` builds as `test_generated_movement` and
  executes production Stride, Step, Stand, Sudden Charge and movement-triggered
  Reactive Strike. It checks diagonal costs across actions and own-turn reset,
  Step preflight without payment, departure versus entry timing, zero-distance
  movement, Stand after removing Prone, reaction MAP exemption and class grants
  independent of weapons. Two subordinate Strides have separate reaction scopes
  while sharing diagonal state and Flourish with Vicious Swing. Suspended choices
  recheck occupancy, original position and reaction grants through exact Undo/replay.
  Maps in these cases are unobstructed flat grids; terrain and creature-space
  sharing are outside the current movement adapter.
- `actions/test_generated_strike.cpp` builds as `test_generated_strike` and loads
  the production compiled Strike through the real factory catalog. It covers
  declared weapons and natural-attack baselines, check outcomes, attack counting,
  critical Fatal/Deadly dice, typed Pack Attack, damage aggregation and defenses,
  target preflight, source snapshots, and suspended execution with rollback.
  Its temporary actor definitions reuse historical JSON profiles; they are not
  complete Remaster fighter/wolf declarations.
  Vicious Swing cases load a declared class feature through the real factory,
  choose equipment without weapon grants, and check same-name source identity,
  level thresholds, MAP, shared Flourish payment, source snapshots and replay.
- `actions/fixtures/generated_actions.ttrpg` and `actions/test_generated_actions.cpp`
  compile actions into `test_generated_actions` and execute them through battle
  grants. They cover chosen entities, dice, arbitrary declared resource fields,
  HP effects, suspended and repeated choices, source removal, actor eligibility,
  and caller-controlled journal rollback. These fixtures are language acceptance
  cases, not the production PF2e action catalog.
- `actions/test_ability_runtime.cpp` covers grant and feature provenance, immutable
  hook execution, source removal during suspension, replay after successful resume
  and rollback, and battle identity/insertion failures. Hook-removal tests replace
  a generated class value through the journal only when none of its child fields
  have retained mutation history; they do not introduce collection mutation APIs.
- `actions/test_reaction_runtime.cpp` builds as `test_reaction_runtime`. It checks
  native typed registry validation and legacy coexistence, readonly requirements,
  explicit decline, source/resource rechecks, preparation before payment, copied
  pending values across nested suspension/replay, per-actor scope across equipment
  sources, independent offer/execution contexts, late callback rejection, trigger
  cycle guards, bounded immediate stack use and completion after parent death or
  grant removal. These tests exercise the native runtime directly. Existing
  `test_abilities` payment tests cover the extracted shared
  payment implementation, including aliases and atomic rollback.
- `actions/fixtures/generated_reactions.ttrpg` and `actions/test_generated_reactions.cpp`
  build as `test_generated_reactions`. They cover compiled typed pending values,
  explicit decline, live requirements before payment, source snapshots and removal,
  nested reaction windows, repeated suspension and exact journal replay. Activity
  scopes share limits across cells and reaction definitions while separate child
  activity calls receive fresh scopes. Tests also check multiple subscriptions,
  independent reaction completion after parent death or grant removal, and native
  body entry capability and current-world validation. These are generic language
  fixtures, not complete PF2e Reactive Strike or Shield Block implementations.
- `actions/test_feature_event_runtime.cpp` builds as `test_feature_events` and
  checks typed immutable handler registration, readonly argument capabilities,
  source deduplication and provenance, snapshot results, context membership,
  stale entity bindings, and rejection of nested or suspending event handlers.
- `actions/test_source_binding_runtime.cpp` builds as `test_source_bindings` and
  checks nearest typed source resolution, immutable execution registration,
  preparation before payment, readonly source snapshots, stable-path replacement,
  actor/source invalidation and repeated resume checks before native callbacks.
  Collection replacement cases retain no earlier child-field journal entries.
- `actions/test_world_runtime.cpp` checks readonly world capabilities, participant
  identity and membership, unbound-world failures, coordinates and the explicit
  Chebyshev grid metric, live geometry across rollback, world-aware preflight before
  payment, legacy requirement adapters, and world propagation into pure events.
- `actions/fixtures/generated_sources.ttrpg` and `actions/test_generated_sources.cpp`
  compile into `test_generated_sources`. They verify named equipment definitions,
  map/set and trait-payload construction, readonly class/collection queries,
  same-name weapon sources, natural weapons, source preflight, and snapshot values
  across removal, replacement, suspension and rollback.
- `actions/fixtures/generated_world.ttrpg` and `actions/test_generated_world.cpp`
  compile into `test_generated_world`. They check target existence before payment,
  live positions, teams and participants in action predicates and pure feature
  events, source reach snapshots, and suspended target validation with rollback.
  Prefix `requires` gates new executions before payment; a paid continuation does
  not rerun it. Existing imperative `require` keeps its body assertion semantics.
  The ally-count fixture is a language test, not a production PF2e action. Its
  line-of-effect check uses the current map adapter, which always reports true.
- `actions/test_mixed_attack_runtime.cpp` runs compiled Strike and the legacy weapon,
  Trip and opportunity pipelines against the same creature state. It checks the
  current weapon's MAP, shared attack counts on hits and misses, non-Agile Trip,
  reaction exemption, generated base damage inputs, turn resets and joint undo.
- `actions/test_legacy_damage_context.cpp` compares real legacy weapon attacks with
  compiled Strike for ordinary, critical and nonlethal knockout under dying/death
  policies. It checks critical Trip, fresh metadata on subsequent attacks, optional
  flat-damage flags, rejection of dead targets before dice, area eligibility and
  generated health-condition adapters with exact journal rollback.
- `actions/fixtures/generated_flow.ttrpg` and `actions/test_generated_flow.cpp`
  compile into `test_generated_flow`. They exercise nested entity/value choices,
  branches, while-loop locals and early returns, repeated suspension and replay,
  and thousands of immediate choices without callback-stack growth. Position
  choices check live occupancy and an explicitly captured movement origin;
  generated variant/resource mutations share the rollback journal and reject
  invalid spends, negative counts and overflow before the failing write. The
  two-step traversal is a generic language fixture, not the PF2e Stride rule.
- `actions/test_movement_runtime.cpp` builds as `test_movement_runtime` and checks
  adjacent position enumeration, map bounds, occupancy, mutable entity capabilities,
  stale origins and grants, failed movement/insertion, and replay with map rollback.
  The generic movement primitive has no resource cost or implicit reaction window;
  legacy `MovePlayer` retains support for movement beyond adjacent cells.
- `actions/fixtures/generated_activities.ttrpg` and `actions/test_generated_activities.cpp`
  compile into `test_generated_activities`. They check nested primitive, enum,
  class, Position and void results across suspension, replay and journal undo; parent costs
  are paid once and immediate calls keep loop stack usage bounded. Source values
  remain snapshots while grant validity stays live. Native entry checks reject
  foreign or readonly entity arguments before effects and retain the original
  actor across context changes. Failed activities preserve completed effects
  until the caller undoes their shared journal.
- `actions/fixtures/generated_source_choices.ttrpg` and
  `actions/test_generated_source_choices.cpp` build as `test_generated_source_choices`.
  They cover equipment/natural-source unions without action grants, exact entry
  identity, pure target preflight, receive-time predicates and snapshots, nested
  activity input validation, source removal and restore, parent scope cleanup,
  current-world membership, context ownership, repeated resume and bounded stack
  depth for immediate choices. Collection replacements retain no prior child
  field mutation records; attribute edits and whole-collection edits use separate
  scenarios and explicitly reversible journals.
- `inventory/`: generated schema type and JSON loading tests.
- `inventory/test_creature_description.cpp` checks generated creature, ancestry and
  class names, typed traits, arbitrary language names and sense descriptions.
  Metadata survives JSON/AST snapshots and sparse collection rollback. Advanced
  weapons use their independent proficiency rank, and display distances derive
  feet from stored cells with checked arithmetic. Sense metadata does not add
  visibility or detection rules.
- `inventory/fixtures/constructor_choices.ttrpg` and `inventory/test_constructor_choices.cpp`
  verify configurable factory reports and actual construction together: dependent
  choices, disabled input rejection and lazy defaults, declared feature pools,
  combination requirements, arbitrary and empty equipment, nested default failures
  before identity allocation, and independent fighter/wolf values. Restored sparse
  collections retain explicit empty selections and their allocator history.
- `inventory/fixtures/rule_entities.ttrpg` and `inventory/test_rule_entities.cpp`:
  compile generated entities into `test_inventory` and verify named factories,
  identity transfer, nested methods, rollback, pure queries, and snapshots with
  collection allocator state.
- `inventory/fixtures/granted_entities.ttrpg` imports `granted_abilities.ttrpg`;
  `inventory/test_granted_entities.cpp` verifies imported ability registration,
  feature and weapon grant traversal, action/reaction distinction, source IDs,
  and provenance preservation through snapshots and removal of one source.
- `inventory/test_proficiency.cpp` verifies generated rank and fixed-value queries,
  map defaults, enum JSON, snapshots, and checked arithmetic. The mechanics tests
  in `mechanics/test_proficiency_calculation.cpp` cover real warrior/wolf bonuses,
  level changes with rollback, fixed contributions, and condition penalties.
- `game_object_logic/test_proficiency_loading.cpp` covers legacy proficiency
  normalization, canonical nested profiles, missing defaults, conflicting input
  locations, invalid enum keys, and integer bounds through the real factory.
- `dsl/`: runtime DSL parser/evaluator/property tests.
- `ast/`: rollback and AST state-comparison tests.

## Build/Test
- Build a focused target, for example `cmake --build build --target test_inventory`.
- Run all registered tests: `ctest --test-dir build --output-on-failure`.
- `test_actions` is registered with CTest through `gtest_discover_tests`; it can also run directly as `./build/pf2e_engine/tests/actions/test_actions`.
- Many tests load `pf2e_engine/data` using generated `cpp_config.h` and `kRootDirPath`.

## Pitfalls
- Some combat tests intentionally stop long battle loops by expecting `TTooManyCallsError`.
- Keep expected RNG rolls and interaction choices in exact sequence.
- When adding data-dependent tests, add only minimal fixture data or reuse existing JSON sources.

## Production Trip and Strike Follow-Ups
- `actions/test_generated_trip.cpp` builds as `test_generated_trip`. It checks four
  Trip outcomes, non-Agile shared MAP, free hands, size, body reach and the ten-foot
  diagonal exception before payment. Critical Trip uses the attack damage window,
  defenses, shield Hardness, temporary HP and critical knockout context.
- Production bound and selected-weapon Strikes publish the same result; Knockdown
  retains exact source/target identity, historical eligibility and its no-MAP rule.
  Cases distinguish failed attacks, newer receipts, source replacement, other
  actors, own intervening actions and subordinate Vicious Swing. Consumed inputs
  remain valid through a shield suspension and source restoration with exact Undo.
- Versatile tests choose base and alternate types before RNG, exercise different
  defenses, recheck a moved target, preserve selected source values and replay
  downstream shield windows without rerolling. These cases do not implement Weapon
  Trip, creature footprints or full obstacle visibility.

## Remaster Acceptance
- `inventory/test_remaster_profiles.cpp` checks the production fighter/wolf statblocks,
  declared feat alternatives, disabled/duplicate choices, readied equipment and hand
  validation, generated grant traversal and JSON/AST snapshots.
- `actions/test_generated_intimidation.cpp` builds as `test_generated_intimidation`.
  It checks auditory versus visual checks, spoken versus signed languages, all four
  outcomes, directional observation overrides, independent actor/target immunity,
  the exact ten-minute deadline, target-end-turn fear decay and legacy adapters.
  Suspended target choices recheck observations/immunity before dice; replay supplies
  the same dice inputs after the restored choice point and preserves exact AST state.
  Knockout drops a shield and frees its hand once through healing, death and Undo.
- `test_lib/generated_combat_fixture.h` is a shared fixture for executing individual
  generated actions with the real factory rules. Its IO scripts asynchronous choice
  requests and optional savepoints; full StartBattle tests need a separate synchronous
  next-action policy. Creature storage uses a deque to keep player references stable.

## Generated Encounter Acceptance
- `actions/test_generated_encounter.cpp` builds as `test_generated_encounter` and
  runs `TBattle.StartBattle` to a winner with one declared `HumanFighter1` and two
  separately constructed `Wolf` entities. Scripted initiative, action selection,
  resource replenishment, shield blocking, Knockdown and a predeparture Reactive
  Strike all run through the battle loop. Exact damage and RNG expectations also
  verify that two wolves cannot supply the two other allies required by Pack Attack.
- A second fight equips a custom martial weapon with no action grants and selects
  General Training, Fleet and Toughness through constructor options. Both fights
  use actor-granted UseStrike and actual weapon choices. They finish with different
  winners and retain independent wolf HP and equipment identities.
- A test-only RoundStart observer captures the existing battle journal and performs
  the same generated BeginRound call as the default phase. The IO observer stores
  the first completed-round checkpoint. After the encounter finishes, Undo restores
  the complete battle AST, and StartBattle replays every remaining turn to the same
  winner and exact final AST. Suspended generated reaction choices are drained by
  the real MakeTurn loop; observer counters are outside the compared game state.

## Contextual Flanking
- `actions/test_generated_flanking.cpp` builds as `test_generated_flanking`. It
  tests the declared closed-edge, single-square geometry convention and the ten-foot
  diagonal reach exception. Small/Medium profiles participate; unsupported creature
  footprints do not acquire inferred flanking geometry.
- Real fighter/wolf profiles exercise a Strike that succeeds only against the
  contextual off-guard AC. Two wolves provide flanking without activating Pack
  Attack. No persistent target AC or condition is changed; shield, frightened and
  Prone combine with the event contribution by their modifier types.
- Ally eligibility uses current ability to act and held/natural melee reach, with
  no action/reaction resource gate or BodyReach fallback. The tests move a flanker
  while Versatile selection is suspended and verify the fresh modifier, then Undo
  and replay the same choice with restored positions and exact AST comparison.
  Fifteen-foot reach rejects the third diagonal before payment; ten-foot reach
  retains its special second-diagonal Strike.

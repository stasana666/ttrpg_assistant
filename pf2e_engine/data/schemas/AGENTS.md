# Engine `.ttrpg` Schemas

Canonical project instructions. CLAUDE.md redirects here.

## Purpose
- Source of truth for generated engine types and the PF2e rules compiled from them. CMake generates C++ into `build/generated/pf2e_engine/...` through `ttrpg_codegen`; see `tools/ttrpg/AGENTS.md` for the language.
- `creature.ttrpg` declares the flat `GameEntity TCreature`; its storage, identity, JSON, AST, queries and grant traversal are all generated. The public `creature.h` only forwards to the generated header.
- Conditions are `Prone`, `Frightened`, `Dying`, `Wounded` and `Unconscious`. Attack history is the separate `TCreature.AttacksMade` counter.

## Build/Test
- `cmake --build build --target pf2e_engine_generated_headers test_ttrpg`, then run data-backed engine tests such as `test_inventory`.

## Pitfalls
- Do not edit `build/generated`.
- A new schema file needs `ttrpg_generate` in `pf2e_engine/src/inventory/CMakeLists.txt`, `pf2e_engine_generated_headers`, target sources and imports.
- Register generated classes with DSL-visible fields through `RegisterDslProperties()` in `pf2e_engine/src/dsl/builtins.cpp`.
- Keep language behavior in `tools/ttrpg/` and keep `pf2e_engine/schemas/schema.json` in sync with JSON input shapes.
- Ability names share one compiler namespace across imported modules, so feature groups need distinct names such as `MovementActions` and `TripActions`.
- `abilities.ttrpg` imports creature types while creature, ancestry, class and weapon modules use only the built-in `Feature`; the factory calls `RegisterAbilitiesRules` explicitly to avoid an import cycle.
- Legacy JSON creatures, weapons and action pipelines keep their historical data and tests. New declarations follow the Remaster rules.

## Core Values
- `hitpoints.ttrpg` owns `Statistic`, `TDamageResult` and `THitPoints`; damage takes the amount after defenses, and temporary HP replacement takes an explicit pool.
- `proficiency.ttrpg` keeps rank arithmetic in `TProficiencyValue.Bonus(Level)`. Missing map entries count as untrained without inserting entries. The factory normalizes legacy lowercase ranks and integer bonuses.
- `statistic_baseline.ttrpg` represents a fixed complete NPC total, including zero and negative values. A fixed total skips attribute and proficiency additions but keeps the proficiency rank for eligibility; a wolf's +9 attack already includes its attributes.
- `statistics.ttrpg` and `lore.ttrpg` define attribute bindings, Class DC and named Lore skills. `Bonus`/`DC` queries merge conditions with explicit `TCheckModifiers`: Item, Status and Circumstance keep their strongest bonus and penalty, untyped penalties stack, and Frightened joins Status.
- `combat_checks.ttrpg` owns the four degrees of success, natural 1/20 shifts and the weapon damage minimum. Clamp penalized damage to the minimum before critical doubling, then add critical-only dice and apply defenses once per damage type.
- Initiative sorts by descending total, then by explicit `TInitiativeOptions.TieOrder`, then insertion. Later stat changes never reroll or reorder.
- `creature_description.ttrpg` adds names, traits, named languages and senses. Movement and reach stay in grid cells; `SpeedFeet` and `ReachFeet` convert for display.

## Abilities and Strike
- `abilities.ttrpg` declares the action catalog and feature groups. `TCreature`, `TRace`, `TClass` and `TWeapon` grant through `collection<Feature>`, and the creature also grants race, class, weapons and natural weapons.
- `BasicStrike` grants `UseStrike`, which picks a current weapon or natural attack; `WeaponStrike` on a weapon grants a bound `Strike`. Both use `ResolveMeleeStrike`, which checks the target before payment and on selection, rolls once, records misses, applies Fatal, Deadly and Versatile, and returns the outcome.
- Multiple attack penalty uses `AttacksMade` and the current weapon's Agile trait, capped after two attacks, and resets at turn boundaries. Vicious Swing counts two attacks; Reactive Strike and monster Knockdown neither apply nor increase it.
- Pack Attack needs two other living allies with reach and line of effect, so two wolves alone cannot trigger it. `ViciousSwing` pays two actions and the shared Flourishes resource and adds one, two or three weapon dice by level.
- `combat_geometry.ttrpg` defines `InReach` as `max(dx, dy) + min(dx, dy) / 2` cells, with the ten-foot reach exception, plus the closed-edge single-cell `Flanks` test. `flanking.Flanking` adds the off-guard penalty through `MeleeTargetModifiers` without changing stored AC or conditions. See [Reach](https://2e.aonprd.com/Rules.aspx?ID=2379) and [Flanking](https://2e.aonprd.com/Rules.aspx?ID=2375).

## Movement, Shields, Trip and Intimidation
- `movement.ttrpg` declares Stride, Step, Stand, Sudden Charge and Reactive Strike. Stride opens `MovementOpening` before leaving each square within one reaction scope. Diagonals alternate one and two cells through `TCreature.DiagonalSteps`, which resets at the creature's own turn end. Step needs two cells of Speed and an affordable free destination. See [movement reactions](https://2e.aonprd.com/Rules.aspx?ID=2355).
- `shield.ttrpg` and `shield_actions.ttrpg` declare `TShield`, Raise a Shield and Shield Block. `ApplyAttackDamage` resolves defenses once, opens `BeforeAttackDamage`, subtracts Hardness once, and both shield and creature take the remainder. The shield lowers in the turn-start expiration phase and drops on knockout.
- `trip.ttrpg` declares Trip, which needs a free hand, uses body reach, Athletics against Reflex DC and non-Agile MAP. Monster Knockdown follows a successful Strike with the same exact source, costs one action and ignores MAP. Strike publishes `StrikeResult` only after damage and reaction windows finish.
- `intimidation.ttrpg` declares Demoralize and Intimidating Glare with a ten-minute immunity relation. `Awareness`, `Visibility` and `Audibility` relations default to true, and scenarios can override them. Frightened keeps its highest value and decreases at the creature's own turn end.
- `combat_timing.ttrpg` sets six seconds per completed round for the encounter clock.

## Health and Turns
- `health.ttrpg` defines `EZeroHpRule`. `Defeat` keeps historical HP-zero defeat; `Dying` adds knockout, dying, wounded and recovery; `Death` kills on lethal damage at zero HP. Nonlethal knockout leaves a creature unconscious.
- `ReceiveDamageWithContext` applies defenses once, and `ApplyResolvedDamage` performs transitions. Critical damage raises dying by two and ordinary damage by one; massive damage uses the resolved blow before temporary HP. See [knockout](https://2e.aonprd.com/Rules.aspx?ID=2324) and [recovery](https://2e.aonprd.com/Rules.aspx?ID=2326).
- A new nonfatal knockout moves the creature before the current turn with `move_before_current_turn`. Unconscious creatures regain resources but cannot use them; dead creatures regain nothing.
- `BeginRound` restores Flourishes; `BeginTurn` restores three actions and one reaction and resets attack counting; `EndTurn` clears them. The factory binds `StartCreatureTurn`, which resolves recovery with `RecoveryDCAdjustment` contributions such as Toughness before `BeginTurn`.

## Remaster Profiles
- `remaster_profiles.ttrpg` defines the production `HumanFighter1` and `Wolf` factories. The fighter's readied weapons, armor, shield, extra language and declared feats are configurable. The default class feat is Sudden Charge, Natural Ambition supplies Vicious Swing, and General Training swaps in Toughness or Fleet.
- Hands come from `TWeapon.HandsRequired` and the wielded shield; a two-handed weapon with a shield is rejected. Wolves have fixed baselines and Knockdown jaws, and each factory call creates independent entities.
- `CombatBasics` grants the five action groups and `Flanking`.
- Reference statblocks: [Fighter](https://2e.aonprd.com/Classes.aspx?ID=35), [Human](https://2e.aonprd.com/Ancestries.aspx?ID=64), [Wolf](https://2e.aonprd.com/Monsters.aspx?ID=3241), [Knockdown](https://2e.aonprd.com/MonsterAbilities.aspx?ID=46), [Vicious Swing](https://2e.aonprd.com/Feats.aspx?ID=4775), [Sudden Charge](https://2e.aonprd.com/Feats.aspx?ID=4774), [Shield Block](https://2e.aonprd.com/Feats.aspx?ID=5212), [Intimidating Glare](https://2e.aonprd.com/Feats.aspx?ID=5162). Expected fighter values: modifiers +4/+2/+2/+0/+1/+0, HP 21, AC 15, Perception +6, Fortitude +7, Reflex +7, Will +4, Class DC 17.

## Not Yet Implemented
- Additional PF2e actions and feat catalogues, ranged and grip rules, multi-cell footprints, terrain, lighting and perception, held-item dropping, contextual modifiers for other check families, cross-encounter persistence, and whole-action cancellation or refunds. JSON pipelines remain adapters for unmigrated content.

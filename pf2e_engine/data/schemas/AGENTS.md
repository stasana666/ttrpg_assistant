# Engine `.ttrpg` Schemas

Canonical project instructions. CLAUDE.md redirects here.

## Purpose
- Source of truth for generated engine types such as armor, material, weapon, damage, dice, creature data, creature parts, and condition.
- Current generated condition schema is a variant with `Prone`, `Frightened`, and `MultipleAttackPenalty`.
- `TCreature` stores passive damage data as `set<EDamageType> Immunities`, `map<EDamageType, int> Resistances`, and `map<EDamageType, int> Vulnerabilities`; damage resolution lives in combat/mechanics code, not on `TCreature`.
- CMake generates C++ into `build/generated/pf2e_engine/...` through `ttrpg_codegen`.

## Key Concepts
- `creature.ttrpg` declares the flat `GameEntity TCreature`; all creature storage, identity, JSON snapshots, AST traversal, characteristic queries, and grant traversal are generated. The public `creature.h` only forwards to the generated `inventory/creature.h`.
- Language forms include `GameEntity`, named instances, `method`, and `query` in addition to the existing fields/containers/defaults. See `tools/ttrpg/AGENTS.md` for the currently supported rule syntax.
- `hitpoints.ttrpg` is the source of truth for `Statistic`, `TDamageResult`, and `THitPoints`: damage, healing, and temporary HP replacement are compiled methods. `TCreature.Hitpoints` constructs this type after race/class/level/attributes are initialized.
- `proficiency.ttrpg` defines proficiency ranks, fixed contributions, and bonus queries. `TProficiencyValue.Bonus(Level)` keeps rank arithmetic in the schema; `TProficiency` stores category maps and perception without storing another level. Missing map entries use an untrained value and do not insert a map entry.
- `skills.ttrpg` and `saving_throws.ttrpg` own `ESkill` and `ESavethrow`; armor and weapon categories remain in their existing schemas. Canonical generated JSON uses PascalCase enum tokens. The factory normalizes legacy lowercase proficiency ranks and fixed integer contributions before loading generated types.
- Damage input is the resolved amount after defenses. The result retains incoming damage, temporary absorption, before/after HP, and remaining damage. Dying/wounded/death rules are not part of this value type.
- Temporary HP replacement accepts an explicitly selected pool; choosing between sources and scheduling expiration remain action/ruleset work.
- Built-ins include `int`, `bool`, `string`, `BoundedQuantity`, `Resource`, `max_int`, and `min_int`.
- `Resource` lowers to the runtime `TResource` (count with `Count`/`Has`/`Add`/`Reduce`); `TCreature` uses it for `Actions`, `Reactions`, `Hands`, and `map<ESpellSlotRank, Resource> SpellSlots`.
- Imports are relative to the importing schema. `ttrpg_generate` in `pf2e_engine/src/inventory/CMakeLists.txt` tracks transitive imports through generated depfiles.
- Generated JSON keys use `PascalToSnake`.
- Class fields load from either string refs through `TGameObjectFactory` or inline JSON objects. New by-name types use `RegisterType<T>(jsonType)`; inline-only classes need no factory registration.
- `collection<Class>` lowers to `TIdCollection<Class>`; `TCreature` currently uses it for weapons and natural weapons.
- `map<Enum, V>` lowers to `std::map`; JSON object keys are parsed through the generated enum `FromString`.

## Build/Test
- Build generated headers: `cmake --build build --target pf2e_engine_generated_headers`.
- Validate language/codegen behavior: `cmake --build build --target test_ttrpg`.
- Then run data-backed engine tests such as `test_inventory`.

## Pitfalls
- Do not edit `build/generated`.
- If adding a schema file, wire it into CMake generation, `pf2e_engine_generated_headers`, target sources, imports, and generated include paths.
- If adding a generated class with DSL-visible fields, register its `RegisterDslProperties()` in `pf2e_engine/src/dsl/builtins.cpp`.
- Keep `.ttrpg` language behavior changes in `tools/ttrpg/`, not here.
- Keep `pf2e_engine/schemas/schema.json` synchronized when JSON input shape changes.

## Ability Grants
- `abilities.ttrpg` declares action symbols and reusable feature groups. `TCreature`, `TRace`, `TClass`, and `TWeapon` store `collection<Feature>` and expose their providers with `grant @self.Features`.
- `TCreature` also grants its race, class, weapon collection, and natural-weapon collection. Traversal reads their current selections; it does not copy action lists during construction.
- `feature WeaponStrike { grant Strike; }` can be selected on any declared weapon. Its pipeline binds that particular weapon as `$source`/`$weapon`; the original `AttackWithWeapon` action retains the legacy weapon-selection prompt.
- Feature choices are arbitrary declared definitions at this stage; ancestry/class prerequisites, level gates, choice counts, contextual modifiers, and the full PF2e feat catalogue are not implemented.
- `tests/inventory/fixtures/granted_entities.ttrpg` shows named fighter/wolf factories with collection literals and imported feature definitions. These are language fixtures, not complete PF2e statblocks.

## Migration Sequence
- Completed: typed methods and queries, implicit journaled mutations, generated hit-point rules, named entity factories, source-bound ability grants, and proficiency rules using the current creature level.
- Completed: fully generated `TCreature`, immutable ability definitions and compatibility hooks owned by the battle ruleset, and generated state loaded through the factory that performs construction.
- Completed: the first compiled action bodies with declared resource costs, entity choices, dice, journaled mutations, and replayable continuations. `tests/actions/fixtures/generated_actions.ttrpg` exercises these mechanisms through the real battle adapter.
- Completed: typed pure feature events and synchronous loops over their contributions. Active features can return primitive, enum, or ordinary class values without action grants; event results preserve source provenance and snapshot semantics across suspension/replay.
- Completed: typed readonly action sources with preflight and snapshots, named value reuse, map/set and variant-payload construction, class-valued parameters, and synchronous collection queries. `tests/actions/fixtures/generated_sources.ttrpg` exercises independent equipment and its bound actions.
- Completed: readonly world queries in actions and feature hooks, `requires` preconditions, `Resource.Count()`, and target-existence preflight for a leading `choose`. `tests/actions/fixtures/generated_world.ttrpg` covers live positions and teams, readonly entity sequences, source snapshots and prepayment-only requirements.
- Next: compiled PF2e combat, character choices, reaction bodies/windows, and declarative replacements for production PF2e action pipelines. JSON pipelines remain a migration adapter until those replacements exist. Choices after prior effects and action cancellation/refund policy still need explicit rule/runtime support.
- Then: contextual statistics and feature modifiers, declaration-time character choices, complete fighter/wolf definitions, and reusable encounter construction with selected weapons and class/ancestry features.
- Final acceptance: construct one human fighter and two independent wolves from declarations, equip different declared weapons and features, enumerate and execute their actions and reactions, and verify selection, resource use, effects, and rollback in combat. The existence of generated entity and grant plumbing alone does not complete this sequence.

## Remaining Integration Boundaries
- `abilities.ttrpg` imports creature types. Creature, ancestry/class and weapon modules depend only on the built-in `Feature`, without importing the action catalog. The factory explicitly calls `RegisterAbilitiesRules`, so production action bodies can use creature types without an import cycle.
- Compiled combat still needs controlled movement/condition/resource operations and choices within composed activities. Keep their PF2e formulas and timing in the rules module. Existing readonly world queries expose generic grid coordinates and Chebyshev distance, without encoding PF2e movement costs.
- Complete character construction still needs choice counts, prerequisites, legal feature selections and configurable named creature construction. Named values and test fixtures alone do not complete that authoring surface.
- NPC final-stat baselines must be separate from fixed proficiency contributions: a wolf's fixed attack +9 already includes its attributes. Keep `TProficiencyValue.FixedBonus` as a proficiency contribution. Legacy JSON baselines remain covered by their own tests while new Remaster profiles receive separate expectations.
- Replace accumulated MAP penalties with attacks made and a penalty determined by the current weapon. Mixed Agile/non-Agile attacks currently expose an incorrect legacy accumulation. Future Vicious Swing must count as two attacks; Knockdown must use an explicit immediately preceding attack record.
- World visibility is incomplete: `TBattleMap::HasLine` currently returns true. A compiled call to that adapter does not by itself implement obstacles or line of effect.
- When migrating damage, clamp a penalized weapon damage roll to its minimum before critical doubling, then add critical-only dice and apply defenses once to each accumulated damage type. General incoming-damage methods must not turn an already resolved zero into one point of damage. The legacy resolver currently clamps after evaluating its combined expression; keep that compatibility behavior separate until its callers migrate.

## Planned PF2e Acceptance Data
- Use the Remaster rules (Player Core and Monster Core) for the new declarations; existing JSON combat fixtures retain their historical statistics until an explicit data/rules migration.
- Human fighter 1: Human ancestry boosts Strength/Dexterity; Versatile Human with Toughness; Warrior background boosts Strength/Constitution, Intimidation, Warfare Lore and Intimidating Glare; Fighter key Strength; free boosts Strength/Dexterity/Constitution/Wisdom. Choose Athletics plus Acrobatics, Medicine and Survival; Natural Ambition grants Vicious Swing, and the normal class feat is Sudden Charge. Human also needs Common and one additional language. Expected modifiers: +4/+2/+2/+0/+1/+0; HP 21; unarmored AC 15; Perception +6; Fortitude +7, Reflex +7, Will +4; Class DC 17. Fighter also grants Reactive Strike and Shield Block.
- Primary rules references: [Human](https://2e.aonprd.com/Ancestries.aspx?ID=64), [Versatile Human](https://2e.aonprd.com/Heritages.aspx?ID=262), [Warrior](https://2e.aonprd.com/Backgrounds.aspx?ID=445), [Fighter](https://2e.aonprd.com/Classes.aspx?ID=35), [Natural Ambition](https://2e.aonprd.com/Feats.aspx?ID=4478), [Toughness](https://2e.aonprd.com/Feats.aspx?ID=5227).
- [Wolf](https://2e.aonprd.com/Monsters.aspx?ID=3241): level 1, Medium Animal, HP 24, AC 15, Perception +7, low-light vision, imprecise scent 30 feet; Acrobatics +7, Athletics +6, Stealth +7, Survival +7; modifiers +2/+4/+1/-4/+2/-2; Fortitude +6, Reflex +9, Will +5; Speed 35 feet; jaws +9, 1d6+2 piercing, Knockdown. Pack Attack adds 1d4 when the target is in reach of two other allies. Two wolves alone do not satisfy this requirement.
- [Remaster Knockdown](https://2e.aonprd.com/MonsterAbilities.aspx?ID=46) is a separate one-action Trip immediately after a qualifying hit, without applying or increasing MAP. It is not automatic prone on a jaws hit.
- [Vicious Swing](https://2e.aonprd.com/Feats.aspx?ID=4775) and [Sudden Charge](https://2e.aonprd.com/Feats.aspx?ID=4774) both cost two actions and have Flourish. Track attacks made separately from their weapon-dependent MAP; Vicious Swing counts as two attacks. Sudden Charge permits two Strides and a melee Strike. Only one Flourish activity per round is allowed.
- [Intimidating Glare](https://2e.aonprd.com/Feats.aspx?ID=5162) changes Demoralize to visual and removes the shared-language penalty. Toughness also changes recovery checks. [Shield Block](https://2e.aonprd.com/Feats.aspx?ID=5212) requires a raised shield and resolves hardness, creature damage and shield damage. A display-only grant does not implement these feats.
- [Longsword](https://2e.aonprd.com/Weapons.aspx?ID=386) uses d8 and versatile piercing; the old JSON longsword uses d6. [Default initiative](https://2e.aonprd.com/Rules.aspx?ID=2423) uses Perception, whereas the legacy calculator currently uses Dexterity. Migrate those behaviors with their combat expectations, not incidentally during entity generation.
- Acceptance covers arbitrary declared weapons and legal declared feature choices using general mechanisms, not the entire published PF2e catalogue.
- Damage rules references: [damage rolls, minimums and critical doubling](https://2e.aonprd.com/Rules.aspx?ID=2302), [order of immunities, weaknesses and resistances](https://2e.aonprd.com/Rules.aspx?ID=2309). Dice added specifically on a critical hit are not doubled; weapon-dice counts exclude extra dice from activities and traits.

# Engine `.ttrpg` Schemas

Canonical project instructions. CLAUDE.md redirects here.

## Purpose
- Source of truth for generated engine types such as armor, material, weapon, damage, dice, creature data, creature parts, and condition.
- Current generated condition schema is a variant with `Prone`, `Frightened`, and `MultipleAttackPenalty`.
- `TCreatureData` stores passive damage data as `set<EDamageType> Immunities`, `map<EDamageType, int> Resistances`, and `map<EDamageType, int> Vulnerabilities`; damage resolution lives in combat/mechanics code, not on `TCreature`.
- CMake generates C++ into `build/generated/pf2e_engine/...` through `ttrpg_codegen`.

## Key Concepts
- Language forms include `GameEntity`, named instances, `method`, and `query` in addition to the existing fields/containers/defaults. See `tools/ttrpg/AGENTS.md` for the currently supported rule syntax.
- `hitpoints.ttrpg` is the source of truth for `Statistic`, `TDamageResult`, and `THitPoints`: damage, healing, and temporary HP replacement are compiled methods. `TCreatureData.Hitpoints` constructs this type after race/class/level/attributes are initialized.
- Damage input is the resolved amount after defenses. The result retains incoming damage, temporary absorption, before/after HP, and remaining damage. Dying/wounded/death rules are not part of this value type.
- Temporary HP replacement accepts an explicitly selected pool; choosing between sources and scheduling expiration remain action/ruleset work.
- Built-ins include `int`, `bool`, `string`, `BoundedQuantity`, `Resource`, `max_int`, and `min_int`.
- `Resource` lowers to the runtime `TResource` (count with `Count`/`Has`/`Add`/`Reduce`); `TCreatureData` uses it for `Actions`, `Reactions`, `Hands`, and `map<ESpellSlotRank, Resource> SpellSlots`.
- Imports are relative to the importing schema. `ttrpg_generate` in `pf2e_engine/src/inventory/CMakeLists.txt` tracks transitive imports through generated depfiles.
- Generated JSON keys use `PascalToSnake`.
- Class fields load from either string refs through `TGameObjectFactory` or inline JSON objects. New by-name types use `RegisterType<T>(jsonType)`; inline-only classes need no factory registration.
- `collection<Class>` lowers to `TIdCollection<Class>`; `TCreatureData` currently uses it for weapons and natural weapons.
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
- `abilities.ttrpg` declares action symbols and reusable feature groups. `TCreatureData`, `TRace`, `TClass`, and `TWeapon` store `collection<Feature>` and expose their providers with `grant @self.Features`.
- `TCreatureData` also grants its race, class, weapon collection, and natural-weapon collection. Traversal reads their current selections; it does not copy action lists during construction.
- `feature WeaponStrike { grant Strike; }` can be selected on any declared weapon. Its pipeline binds that particular weapon as `$source`/`$weapon`; the original `AttackWithWeapon` action retains the legacy weapon-selection prompt.
- Feature choices are arbitrary declared definitions at this stage; ancestry/class prerequisites, level gates, choice counts, contextual modifiers, and the full PF2e feat catalogue are not implemented.
- `tests/inventory/fixtures/granted_entities.ttrpg` shows named fighter/wolf factories with collection literals and imported feature definitions. These are language fixtures, not complete PF2e statblocks.

# Engine `.ttrpg` Schemas

Canonical documentation file. AGENTS.md must remain semantically equivalent.

## Purpose
- Source of truth for generated engine types such as armor, material, weapon, damage, dice, creature data, creature parts, and condition.
- Current generated condition schema is a variant with `Prone`, `Frightened`, and `MultipleAttackPenalty`.
- `TCreatureData` stores passive damage data as `set<EDamageType> Immunities`, `map<EDamageType, int> Resistances`, and `map<EDamageType, int> Vulnerabilities`; damage resolution lives in combat/mechanics code, not on `TCreature`.
- CMake generates C++ into `build/generated/pf2e_engine/...` through `ttrpg_codegen`.

## Key Concepts
- Language forms: `import`, `enum`, `variant`, `class`, scalar fields, `set<T>`, `collection<T>`, `map<K, V>`, defaults, and `derive` computed fields.
- Built-ins include `int`, `bool`, `string`, `BoundedQuantity`, `max_int`, and `min_int`.
- Imports are relative to this directory. CMake dependency lists in `pf2e_engine/src/inventory/CMakeLists.txt` must include imported schemas.
- Generated JSON keys use `PascalToSnake`.
- Class fields load from either string refs through `TGameObjectFactory` or inline JSON objects; referenced classes need factory plumbing even if normally authored inline.
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

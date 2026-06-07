# Engine Data

Canonical documentation file. AGENTS.md must remain semantically equivalent.

## Purpose
- Source game data consumed by `TGameObjectFactory`: actions, inventory, creatures, battle maps, and `.ttrpg` schemas.
- JSON data is loaded recursively from this tree by tests, the assistant, and the analyzer.

## Key Files
- `schemas/*.ttrpg`: source definitions for generated inventory, condition, creature-data, and related C++ types.
- `actions/*.json`: action pipelines using runtime DSL expressions and action-block function names.
- `inventory/**`, `creatures/**`, `battle_maps/**`: JSON objects with top-level `type`, `name`, and a matching `pf2e_*` payload.
- `../schemas/schema.json`: JSON Schema used by `tools/data_validation.py`.

## Build/Test
- Validate JSON: `python3 tools/data_validation.py -s pf2e_engine/schemas/schema.json -t pf2e_engine/data`.
- For inventory/schema-backed data, build/run `test_inventory`.
- For action data, build/run `test_actions`, `test_wolf_combat`, and related runtime DSL tests.

## Conventions
- JSON field names are snake_case, generated from `.ttrpg` PascalCase field names by codegen conventions.
- References to other game objects are usually string ids registered by `TGameObjectIdManager`.
- Keep JSON Schema and `.ttrpg` schema expectations aligned when changing data shape.

## Do Not
- Do not change generated C++ to make data load; change schema/codegen/source JSON instead.
- Do not add large binary assets here; assistant images and models live elsewhere.

# Engine Data

Canonical project instructions. CLAUDE.md redirects here.

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
- Creature data, race, class, and weapon payloads accept `features: ["module.FeatureName"]`. Warrior and wolf use the feature groups declared in `schemas/abilities.ttrpg`.
- Creature `weapons` and `natural_weapons` entries are either named references or inline weapon payloads (`name`, `base_dice_size`, `damage_type`, `category`, optional `features`/traits/reach); inline entries do not include a `pf2e_weapon` envelope.
- An action payload can bind a compiled symbol with `ability: "abilities.Strike"`; `source: "weapon"` requires a grant originating from a current weapon/natural-weapon entry. These are transitional pipeline bindings for actions only; reactions are declared as typed `.ttrpg` reactions.
- Existing creature-envelope `actions` arrays are normalized to feature selections by the factory. Actions without a compiled symbol receive `legacy_action.<name>` definitions and `legacy_feature.<name>` wrappers. The factory builds a shared immutable catalogue after definitions have loaded and reports unknown or missing bindings.
- Generated proficiency lives in `creature_data.proficiency`: maps `armor_category`, `weapon_category`, `savethrow`, `skill`, plus `perception`. Each value is an object such as `{"rank": "Expert"}` or `{"is_fixed": true, "fixed_bonus": -2}`. Omitted entries default to untrained, including perception. Integer overrides are proficiency contributions; the calculator adds the appropriate ability modifier separately.
- Legacy envelope `proficiency` remains accepted with lowercase rank strings, integer bonuses, and the old `Perception` key. The factory normalizes this shape before generated loading; defining proficiency in both locations is an error. Numeric overrides must fit `int` and are never combined with the creature's level by the adapter.
- Proficiency snapshots contain rank/fixed data only. Level changes affect ranked queries immediately, without recomputing stored hit-point defaults or applying level-up rules implicitly.

## Do Not
- Do not change generated C++ to make data load; change schema/codegen/source JSON instead.
- Do not add large binary assets here; assistant images and models live elsewhere.

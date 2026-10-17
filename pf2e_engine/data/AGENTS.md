# Engine Data

Canonical project instructions. CLAUDE.md redirects here.

## Purpose
- All game data is declared in `schemas/*.ttrpg`: rules, creatures, items, abilities, actions and map layouts. Production creatures are the `remaster_profiles.ttrpg` constructors; the assistant's map is `battle_maps.ttrpg`.

## Build/Test
- Check a schema without building: `./build/tools/ttrpg/ttrpg_codegen --schema pf2e_engine/data/schemas/remaster_profiles.ttrpg --check`.

## Conventions
- Proficiency values contain rank/fixed data only. Level changes affect ranked queries immediately, without recomputing stored hit-point defaults or applying level-up rules implicitly.

## Do Not
- Do not edit generated C++; change the schema or the generator instead.
- Do not add large binary assets here; assistant images and models live elsewhere.

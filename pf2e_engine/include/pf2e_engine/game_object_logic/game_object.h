#pragma once

#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/creature.h>

#include <variant>

using TGameObject = std::variant<
    TArmor,
    TWeapon,
    TCreature
>;

static_assert(sizeof(TGameObject) == 256);

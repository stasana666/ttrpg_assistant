#pragma once

#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/success_level.h>

#include <variant>

using TGameObjectPtr = std::variant<
    TArmor*,
    TWeapon*,
    TCreature*,
    TPlayer*,
    std::shared_ptr<TDamage>,
    std::string,
    ESuccessLevel
>;

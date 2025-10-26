#pragma once

#include <pf2e_engine/success_level.h>
#include <pf2e_engine/mechanics/damage.h>

#include <variant>

class TArmor;
class TWeapon;
class TCreature;
class TPlayer;

using TGameObjectPtr = std::variant<
    TArmor*,
    TWeapon*,
    TCreature*,
    TPlayer*,
    std::shared_ptr<TDamage>,
    std::string,
    ESuccessLevel
>;

#pragma once

#include <pf2e_engine/success_level.h>
#include <pf2e_engine/mechanics/damage.h>

#include <variant>
#include <vector>

class TArmor;
class TWeapon;
class TCreature;
class TPlayer;

using TPlayerList = std::vector<TPlayer*>;

using TGameObjectPtr = std::variant<
    TArmor*,
    TWeapon*,
    TCreature*,
    TPlayer*,
    TPlayerList,
    std::shared_ptr<TDamage>,
    std::string,
    int,
    ESuccessLevel
>;

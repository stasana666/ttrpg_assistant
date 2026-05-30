#pragma once

#include <pf2e_engine/success_level.h>
#include <pf2e_engine/mechanics/damage.h>
#include <pf2e_engine/dsl/value.h>

#include <variant>
#include <vector>

class TArmor;
class TWeapon;
class TCreature;
class TPlayer;

using TPlayerList = std::vector<TPlayer*>;
using TWeaponList = std::vector<TWeapon*>;

using TGameObjectPtr = std::variant<
    TArmor*,
    TWeapon*,
    TCreature*,
    TPlayer*,
    TPlayerList,
    TWeaponList,
    TDslValue::TListPtr,
    std::shared_ptr<TDamage>,
    std::string,
    int,
    ESuccessLevel
>;

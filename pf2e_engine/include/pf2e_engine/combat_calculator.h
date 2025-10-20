#pragma once

#include <pf2e_engine/game_object_logic/game_object.h>
#include <pf2e_engine/success_level.h>
#include <pf2e_engine/random.h>

class TCombatCalculator {
public:
    int ArmorClass(const TCreature& creature) const;
    int AttackRollBonus(const TCreature& creature, const TWeapon& weapon) const;
    ESuccessLevel RollD20(IRandomGenerator* roller, int bonus, int ac) const;
};

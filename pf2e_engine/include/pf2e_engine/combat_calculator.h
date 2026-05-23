#pragma once

#include <pf2e_engine/game_object_logic/game_object.h>
#include <pf2e_engine/random.h>
#include <pf2e_engine/success_level.h>
#include <pf2e_engine/proficiency.h>

class TCombatCalculator {
public:
    int InitiativeBonus(const TCreature& creature) const;
    int ArmorClass(const TCreature& target, const TCreature& attacker) const;

    // Off-Guard is a per-pair combat state, not a stored condition: a creature
    // can be Off-Guard against one attacker and not another (flanking, etc.).
    bool IsOffGuardFor(const TCreature& target, const TCreature& attacker) const;

    template <class T>
    int DifficultyClass(const TCreature& creature, T type) const;
    template <class T>
    int RollBonus(const TCreature& creature, T type) const;
    template <class T>
    int Penalty(const TCreature& creature, T type) const;

    int AttackRollBonus(const TCreature& creature, const TWeapon& weapon) const;

    ESuccessLevel RollD20(IRandomGenerator* roller, int bonus, int ac) const;
};

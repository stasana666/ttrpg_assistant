#pragma once

#include <pf2e_engine/action_blocks/base_function.h>
#include <pf2e_engine/combat_calculator.h>

class FWeaponDamageRoll : public FBaseFunction {
public:
    FWeaponDamageRoll(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator() (TActionContext& ctx) const;

protected:
    // TCombatCalculator calculator_;
};

class FCritWeaponDamageRoll : public FWeaponDamageRoll {
public:
    FCritWeaponDamageRoll(TBlockInput&& input, TGameObjectId output)
        : FWeaponDamageRoll(std::move(input), output) {}

    void operator() (TActionContext& ctx) const;

private:
    // TCombatCalculator calculator_;
};

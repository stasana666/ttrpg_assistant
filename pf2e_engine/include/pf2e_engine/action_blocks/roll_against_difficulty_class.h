#pragma once

#include <pf2e_engine/action_blocks/base_function.h>
#include <pf2e_engine/combat_calculator.h>

class FRollAgainstDifficultyClass : public FBaseFunction {
public:
    FRollAgainstDifficultyClass(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator() (TActionContext& ctx) const;

private:
    void WeaponAttackHandle(TActionContext& ctx) const;
    void SkillHandle(TActionContext& ctx) const;

    TCombatCalculator calculator_;
};

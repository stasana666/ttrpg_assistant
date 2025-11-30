#pragma once

#include <pf2e_engine/action_blocks/base_function.h>
#include <pf2e_engine/combat_calculator.h>

class FRollAgainstDifficultyClass : public FBaseFunction {
public:
    FRollAgainstDifficultyClass(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator() (std::shared_ptr<TActionContext> ctx) const;

private:
    void WeaponAttackHandle(std::shared_ptr<TActionContext> ctx) const;
    void SkillHandle(std::shared_ptr<TActionContext> ctx) const;

    TCombatCalculator calculator_;
};

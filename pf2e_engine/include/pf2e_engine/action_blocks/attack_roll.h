#pragma once

#include <pf2e_engine/action_blocks/base_function.h>
#include <pf2e_engine/combat_calculator.h>

class FAttackRoll : public FBaseFunction {
public:
    FAttackRoll(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator() (TActionContext& ctx) const;

private:
    TCombatCalculator calculator_;
};

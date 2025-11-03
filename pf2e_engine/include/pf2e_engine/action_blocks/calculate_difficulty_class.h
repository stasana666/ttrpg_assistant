#pragma once

#include <pf2e_engine/action_blocks/base_function.h>
#include <pf2e_engine/combat_calculator.h>

class FCalculateDifficultyClass : public FBaseFunction {
public:
    FCalculateDifficultyClass(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator() (TActionContext& ctx) const;

private:
    void ArmorClassHandle(TActionContext& ctx) const;
    void SkillDifficultyClassHandle(TActionContext& ctx) const;

    TCombatCalculator calculator_;
};

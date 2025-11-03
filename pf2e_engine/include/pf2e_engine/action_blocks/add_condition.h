#pragma once

#include <pf2e_engine/action_blocks/base_function.h>

class FAddCondition : public FBaseFunction {
public:
    FAddCondition(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator ()(TActionContext& ctx) const;

private:
    void MultipleAttackPenaltyHandle(TActionContext& ctx) const;
    void FrightenedHandle(TActionContext& ctx) const;
};

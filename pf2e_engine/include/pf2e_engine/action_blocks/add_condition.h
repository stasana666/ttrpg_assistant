#pragma once

#include <pf2e_engine/action_blocks/base_function.h>

class FAddCondition : public FBaseFunction {
public:
    FAddCondition(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator ()(std::shared_ptr<TActionContext> ctx) const;

private:
    void MultipleAttackPenaltyHandle(std::shared_ptr<TActionContext> ctx) const;
    void FrightenedHandle(std::shared_ptr<TActionContext> ctx) const;
};

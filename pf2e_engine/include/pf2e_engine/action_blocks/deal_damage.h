#pragma once

#include <pf2e_engine/action_blocks/base_function.h>

class FDealDamage : public FBaseFunction {
public:
    FDealDamage(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator() (std::shared_ptr<TActionContext> ctx) const;
};

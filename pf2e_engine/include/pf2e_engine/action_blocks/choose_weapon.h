#pragma once

#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/action_blocks/base_function.h>

class FChooseWeapon : public FBaseFunction {
public:
    FChooseWeapon(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator() (TActionContext& ctx) const;
};

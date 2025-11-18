#pragma once

#include <pf2e_engine/action_blocks/base_function.h>

class FMove : public FBaseFunction {
public:
    FMove(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator() (TActionContext& ctx) const;
};

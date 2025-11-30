#pragma once

#include <pf2e_engine/action_blocks/base_function.h>

class FGetParameter : public FBaseFunction {
public:
    FGetParameter(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator() (std::shared_ptr<TActionContext> ctx) const;

private:
    void MovementHandle(std::shared_ptr<TActionContext> ctx) const;
};

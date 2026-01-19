#pragma once

#include <pf2e_engine/action_blocks/base_function.h>

class FSpellDamageRoll : public FBaseFunction {
public:
    FSpellDamageRoll(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator()(std::shared_ptr<TActionContext> ctx) const;
};

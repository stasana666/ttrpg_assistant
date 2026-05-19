#pragma once

#include <pf2e_engine/action_blocks/base_function.h>

class FMove : public FBaseFunction {
public:
    FMove(TBlockInput&& input, TGameObjectId output)
        : FBaseFunction(std::move(input), output) {}

    void operator() (std::shared_ptr<TActionContext> ctx) const;

private:
    // Performs one step of movement and reports the resulting OnMove reaction
    // opportunity. Decrements `budget`, or sets it to 0 when the player stops.
    void Step(TPlayer& target, int& budget, std::shared_ptr<TActionContext> ctx) const;
};

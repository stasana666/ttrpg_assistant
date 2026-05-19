#include "save_point.h"

#include <pf2e_engine/transformation/transformator.h>

TSavepointStackUnwind::TSavepointStackUnwind(TState state, TSavepointCallback&& callback)
    : state_(state)
    , callback_(std::move(callback))
{
}

void TSavepointStackUnwind::Revert(TTransformator& transformator) const
{
    transformator.Undo(state_);
}

void TSavepointStackUnwind::Resume() const
{
    callback_();
}

void TSavepointStackUnwind::AddCallFunctionLevel(std::function<void(TSavepointCallback)> callback_modifier)
{
    callback_ = [callback = std::move(callback_), modifier = std::move(callback_modifier)]() {
        modifier(callback);
    };
}

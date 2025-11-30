#include "save_point.h"

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

TReactionStackUnwind::TReactionStackUnwind(TState state, TSavepointCallback&& callback, TTriggerContext ctx)
    : TSavepointStackUnwind(state, std::move(callback))
    , ctx_(ctx)
{
}

const TTriggerContext& TReactionStackUnwind::TriggeredContext() const
{
    return ctx_;
}

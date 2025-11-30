#pragma once

#include <pf2e_engine/transformation/transformator.h>

#include <functional>

using TSavepointCallback = std::function<void()>;

class TSavepointStackUnwind {
public:
    TSavepointStackUnwind(TState state, TSavepointCallback&& callback);

    void Revert(TTransformator& transformator) const;
    void Resume() const;
    void AddCallFunctionLevel(std::function<void(TSavepointCallback)> callback_modifier);

protected:
    TState state_;
    TSavepointCallback callback_;
};

class TReactionStackUnwind : public TSavepointStackUnwind {
public:
    TReactionStackUnwind(TState state, TSavepointCallback&& callback, TTriggerContext ctx);

    const TTriggerContext& TriggeredContext() const;

private:
    TTriggerContext ctx_;
};

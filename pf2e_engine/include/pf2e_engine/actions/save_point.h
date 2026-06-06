#pragma once

#include <pf2e_engine/transformation/state.h>

#include <functional>

class TTransformator;

using TSavepointCallback = std::function<void()>;

class TSavepointStackUnwind {
public:
    TSavepointStackUnwind(TState state, TSavepointCallback&& callback);

    void Revert(TTransformator& transformator) const;
    void Resume() const;
    void AddCallFunctionLevel(std::function<void(TSavepointCallback)> callback_modifier);

private:
    TState state_;
    TSavepointCallback callback_;
};

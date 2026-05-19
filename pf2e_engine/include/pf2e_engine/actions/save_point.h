#pragma once

#include <pf2e_engine/transformation/state.h>

#include <functional>

class TTransformator;

using TSavepointCallback = std::function<void()>;

// Exception thrown to suspend gameplay execution. As it unwinds the stack, each
// continuation-aware frame appends "the rest of its work" via
// AddCallFunctionLevel. The top-level handler can later Revert the engine to the
// captured state and Resume() the accumulated continuation, re-entering every
// frame at its suspension point.
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

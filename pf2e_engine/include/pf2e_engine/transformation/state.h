#pragma once

#include <cstddef>

class TTransformator;

// An opaque snapshot of TTransformator's transformation-stack depth.
// Created only by TTransformator::CurrentState() and consumed by Undo() to
// roll the transformator back to that point.
class TState {
private:
    friend class TTransformator;

    explicit TState(std::size_t stack_size);

    std::size_t stack_size_;
};

#pragma once

#include <cstddef>

class TTransformator;

class TState {
private:
    friend class TTransformator;

    explicit TState(std::size_t stack_size, const TTransformator* owner);

    std::size_t stack_size_;
    const TTransformator* owner_;
};

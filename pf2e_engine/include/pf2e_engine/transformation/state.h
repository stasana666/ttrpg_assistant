#pragma once

#include <cstddef>

class TTransformator;

class TState {
private:
    friend class TTransformator;

    explicit TState(std::size_t stack_size);

    std::size_t stack_size_;
};

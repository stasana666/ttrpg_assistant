#pragma once

#include <chrono>

#include <pf2e_engine/position.h>

template <class T>
struct TInteractionEvent {
    using Timepoint = std::chrono::time_point<std::chrono::steady_clock>;

    T value;
    Timepoint timepoint;
};

using TClickEvent = TInteractionEvent<TPosition>;
using TIndexEvent = TInteractionEvent<int>;

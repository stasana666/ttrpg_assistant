#pragma once

#include <pf2e_engine/player.h>

#include <chrono>

struct TClickEvent {
    using Timepoint = std::chrono::time_point<std::chrono::steady_clock>;

    TPosition position;
    Timepoint timepoint;
};

#pragma once

#include <pf2e_engine/creature.h>

struct TPosition {
    size_t x;
    size_t y;
};

struct TPlayer {
    int team;
    TPosition position;
    TCreature* creature;
    std::string name;
};

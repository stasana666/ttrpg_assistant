#pragma once

#include <pf2e_engine/creature.h>

struct TPosition {
    size_t x;
    size_t y;
};

class TPlayer {
public:
    int team;
    int id;
    TPosition position;
    TCreature* creature;
    std::string name;
};

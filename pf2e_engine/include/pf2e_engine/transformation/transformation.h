#pragma once

#include <pf2e_engine/mechanics/hitpoints.h>
#include <variant>

class TChangeHitPoints {
public:
    TChangeHitPoints(THitPoints* hitpoints, int value);

    void Undo();

private:
    THitPoints* hitpoints_;
    THitPoints prev_;
};

using TTransformation = std::variant<
    TChangeHitPoints
>;

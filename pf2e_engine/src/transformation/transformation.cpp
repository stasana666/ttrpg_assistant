#include <pf2e_engine/transformation/transformation.h>

TChangeHitPoints::TChangeHitPoints(THitPoints* hitpoints, int value)
    : hitpoints_(hitpoints)
    , prev_(*hitpoints_)
{
    if (value < 0) {
        hitpoints_->ReduceHp(-value);
    } else {
        hitpoints_->RestoreHp(value);
    }
}

void TChangeHitPoints::Undo()
{
    *hitpoints_ = prev_;
}

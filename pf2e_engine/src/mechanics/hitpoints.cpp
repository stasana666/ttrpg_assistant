#include "hitpoints.h"

#include <stdexcept>

THitPoints::THitPoints(int max_hp)
    : max_hp_(max_hp)
    , current_hp_(max_hp)
    , temporary_hp_(0)
{
}

int THitPoints::GetCurrentHp() const
{
    return current_hp_ + temporary_hp_;
}

void THitPoints::ReduceHp(int damage)
{
    if (damage < 0) {
        throw std::logic_error("negative damage");
    }
    temporary_hp_ -= damage;
    if (temporary_hp_ < 0) {
        current_hp_ += temporary_hp_;
        temporary_hp_ = 0;
        if (current_hp_ < 0) {
            current_hp_ = 0;
        }
    }
}

void THitPoints::RestoreHp(int heal)
{
    if (heal < 0) {
        throw std::logic_error("negative heal");
    }
    current_hp_ += heal;
    if (current_hp_ > max_hp_) {
        current_hp_ = max_hp_;
    }
}

void THitPoints::SetTemporaryHp(int hp)
{
    if (temporary_hp_ < hp) {
        temporary_hp_ = hp;
    }
}

int THitPoints::GetTemporaryHp() const
{
    return temporary_hp_;
}

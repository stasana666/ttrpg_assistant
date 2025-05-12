#include "hitpoints.h"

#include <stdexcept>

THitPoints::THitPoints(int maxHp)
    : MaxHp(maxHp)
    , CurrentHp(maxHp)
    , TemporaryHp(0)
{
}

int THitPoints::GetCurrentHp() const
{
    return CurrentHp + TemporaryHp;
}

void THitPoints::ReduceHp(int damage)
{
    if (damage < 0) {
        throw std::logic_error("negative damage");
    }
    TemporaryHp -= damage;
    if (TemporaryHp < 0) {
        CurrentHp += TemporaryHp;
        TemporaryHp = 0;
        if (CurrentHp < 0) {
            CurrentHp = 0;
        }
    }
}

void THitPoints::RestoreHp(int heal)
{
    if (heal < 0) {
        throw std::logic_error("negative heal");
    }
    CurrentHp += heal;
    if (CurrentHp > MaxHp) {
        CurrentHp = MaxHp;
    }
}

void THitPoints::SetTemporaryHp(int hp)
{
    if (TemporaryHp < hp) {
        TemporaryHp = hp;
    }
}

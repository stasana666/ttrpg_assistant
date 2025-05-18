#include "hitpoints.h"

#include <stdexcept>

THitPoints::THitPoints(int max_hp)
    : max_hp(max_hp)
    , current_hp(max_hp)
    , temporary_hp(0)
{
}

int THitPoints::GetCurrentHp() const
{
    return current_hp + temporary_hp;
}

void THitPoints::ReduceHp(int damage)
{
    if (damage < 0) {
        throw std::logic_error("negative damage");
    }
    temporary_hp -= damage;
    if (temporary_hp < 0) {
        current_hp += temporary_hp;
        temporary_hp = 0;
        if (current_hp < 0) {
            current_hp = 0;
        }
    }
}

void THitPoints::RestoreHp(int heal)
{
    if (heal < 0) {
        throw std::logic_error("negative heal");
    }
    current_hp += heal;
    if (current_hp > max_hp) {
        current_hp = max_hp;
    }
}

void THitPoints::SetTemporaryHp(int hp)
{
    if (temporary_hp < hp) {
        temporary_hp = hp;
    }
}

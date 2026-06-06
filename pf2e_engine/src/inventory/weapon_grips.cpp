#include <pf2e_engine/inventory/weapon_grips.h>

#include <pf2e_engine/inventory/weapon.h>

std::vector<int> Grips(const TWeapon& ) {
    return {1};
}

bool ValidGrip(const TWeapon& weapon, int hand_count) {
    for (int g : Grips(weapon)) {
        if (g == hand_count) {
            return true;
        }
    }
    return false;
}

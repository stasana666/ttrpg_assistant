#pragma once

#include <pf2e_engine/mechanics/damage.h>
#include <vector>
#include "item.h"

class TWeapon : public TItem {
public:
    TWeapon(int base_dice_size, TDamage::Type type);

    int GetBaseDiceSize() const;
    TDamage::Type GetDamageType() const;
    std::vector<int> Grips() const;
    bool ValidGrip(int hand_count) const;

private:
    int base_dice_size_;
    TDamage::Type type_;
};

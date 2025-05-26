#pragma once

#include <pf2e_engine/mechanics/damage.h>
#include "item.h"

class TWeapon : public TItem {
public:
    TWeapon(int base_die_size, TDamage::Type type);

    int GetBaseDieSize() const;
    TDamage::Type GetDamageType() const;

private:
    int base_die_size;
    TDamage::Type type;
};

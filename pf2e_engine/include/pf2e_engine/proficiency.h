#pragma once

#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/weapon.h>

enum class EProficiencyLevel {
    untrained,
    trained,
    expert,
    master,
    legendary,
};

class TProficiency {
public:
    explicit TProficiency(int level);

    int GetProficiency(const TWeapon&) const;
    int GetProficiency(const TArmor&) const;
    int GetLevel() const;

private:
    int level_;
};

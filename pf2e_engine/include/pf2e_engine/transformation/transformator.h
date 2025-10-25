#pragma once

#include <pf2e_engine/transformation/transformation.h>
#include <pf2e_engine/creature.h>

class TTransformator {
public:
    void DealDamage(TCreature* creature, int damage);
    void Heal(TCreature* creature, int value);

    void Undo();

private:
    std::vector<TTransformation> transformations_;
};

#pragma once

#include <pf2e_engine/transformation/transformation.h>
#include <pf2e_engine/interaction_system.h>
#include <pf2e_engine/creature.h>

class TTransformator {
public:
    explicit TTransformator(TInteractionSystem& io_system);

    void DealDamage(TPlayer* player, int damage);
    void Heal(TPlayer* player, int value);

    void Undo();

private:
    TInteractionSystem& io_system_;
    std::vector<TTransformation> transformations_;
};

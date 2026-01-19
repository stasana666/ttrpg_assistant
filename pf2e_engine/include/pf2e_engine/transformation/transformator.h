#pragma once

#include <pf2e_engine/transformation/transformation.h>
#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/creature.h>

class TState {
private:
    friend class TTransformator;

    explicit TState(size_t stack_size);

    size_t stack_size_;
};

class TTransformator {
public:
    explicit TTransformator(IInteractionSystem& io_system);

    void DealDamage(TPlayer* player, int damage);
    void Heal(TPlayer* player, int value);

    void Undo(TState state);

    TState CurrentState() const;

private:
    IInteractionSystem& io_system_;
    std::vector<TTransformation> transformations_;
};

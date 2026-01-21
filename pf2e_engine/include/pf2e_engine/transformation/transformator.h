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

    void ChangeCondition(TCreature* creature, ECondition condition, int new_value);
    void AddResource(TResourcePool* pool, TResourceId id, int count);
    void ReduceResource(TResourcePool* pool, TResourceId id, int count);

    void AddEffect(TEffectManager* manager, TPlayer* player, ECondition condition, int value);
    void RemoveEffect(TEffectManager* manager, TPlayer* player, ECondition condition, int value);

    TTaskId AddTask(TTaskScheduler* scheduler, TTask task);
    void RemoveTask(TTaskScheduler* scheduler, TTaskId id, TTask task, size_t progress_index);
    void AdvanceTaskProgress(TTaskScheduler* scheduler, TTaskId id, size_t new_index);

    void ChangeCurrentPlayer(TInitiativeOrder* order, size_t new_position);
    void ChangeRound(TInitiativeOrder* order, size_t new_round);

    void Undo(TState state);

    TState CurrentState() const;

private:
    IInteractionSystem& io_system_;
    std::vector<TTransformation> transformations_;
};

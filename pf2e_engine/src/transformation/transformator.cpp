#include <pf2e_engine/transformation/transformator.h>
#include <pf2e_engine/common/visit.h>
#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/player.h>

TState::TState(size_t stack_size)
    : stack_size_(stack_size)
{
}

TTransformator::TTransformator(IInteractionSystem& io_system)
    : io_system_(io_system)
{
}

void TTransformator::DealDamage(TPlayer* player, int damage)
{
    TCreature* creature = player->GetCreature();
    transformations_.emplace_back(TChangeHitPoints(&creature->Hitpoints(), -damage));
    io_system_.GameLog() << player->GetName() << " takes " << damage << " amount of damage" << std::endl;
    io_system_.GameLog() << "current hp: " << creature->Hitpoints().GetCurrentHp() << std::endl;
}

void TTransformator::Heal(TPlayer* player, int value)
{
    TCreature* creature = player->GetCreature();
    transformations_.emplace_back(TChangeHitPoints(&creature->Hitpoints(), value));
    io_system_.GameLog() << player->GetName() << " takes " << value << " amount of heal" << std::endl;
    io_system_.GameLog() << "current hp: " << creature->Hitpoints().GetCurrentHp() << std::endl;
}

void TTransformator::ChangeCondition(TCreature* creature, ECondition condition, int new_value)
{
    transformations_.emplace_back(TChangeCondition(creature, condition, new_value));
}

void TTransformator::AddResource(TResourcePool* pool, TResourceId id, int count)
{
    transformations_.emplace_back(TChangeResource(pool, id, count));
}

void TTransformator::ReduceResource(TResourcePool* pool, TResourceId id, int count)
{
    transformations_.emplace_back(TChangeResource(pool, id, -count));
}

void TTransformator::AddEffect(TEffectManager* manager, TPlayer* player, ECondition condition, int value)
{
    transformations_.emplace_back(TAddEffect(manager, player, condition, value));
}

void TTransformator::RemoveEffect(TEffectManager* manager, TPlayer* player, ECondition condition, int value)
{
    transformations_.emplace_back(TRemoveEffect(manager, player, condition, value));
}

TTaskId TTransformator::AddTask(TTaskScheduler* scheduler, TTask task)
{
    auto& transformation = transformations_.emplace_back(TAddTask(scheduler, std::move(task)));
    return std::get<TAddTask>(transformation).GetTaskId();
}

void TTransformator::RemoveTask(TTaskScheduler* scheduler, TTaskId id, TTask task, size_t progress_index)
{
    transformations_.emplace_back(TRemoveTask(scheduler, id, std::move(task), progress_index));
}

void TTransformator::AdvanceTaskProgress(TTaskScheduler* scheduler, TTaskId id, size_t new_index)
{
    transformations_.emplace_back(TAdvanceTaskProgress(scheduler, id, new_index));
}

void TTransformator::ChangeCurrentPlayer(TInitiativeOrder* order, size_t new_position)
{
    transformations_.emplace_back(TChangeCurrentPlayer(order, new_position));
}

void TTransformator::ChangeRound(TInitiativeOrder* order, size_t new_round)
{
    transformations_.emplace_back(TChangeRound(order, new_round));
}

void TTransformator::Undo(TState state)
{
    while (transformations_.size() > state.stack_size_) {
        std::visit(VisitorHelper{
            [&](auto& v) {
                v.Undo();
            }
        }, transformations_.back());
        transformations_.pop_back();
    }
}

TState TTransformator::CurrentState() const
{
    return TState{transformations_.size()};
}

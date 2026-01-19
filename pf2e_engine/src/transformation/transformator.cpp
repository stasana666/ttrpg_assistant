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

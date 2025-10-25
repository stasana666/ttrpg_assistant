#include <pf2e_engine/transformation/transformator.h>
#include  <pf2e_engine/common/visit.h>

void TTransformator::DealDamage(TCreature* creature, int damage)
{
    transformations_.emplace_back(TChangeHitPoints(&creature->Hitpoints(), -damage));
}

void TTransformator::Heal(TCreature* creature, int value)
{
    transformations_.emplace_back(TChangeHitPoints(&creature->Hitpoints(), value));
}

void TTransformator::Undo()
{
    while (!transformations_.empty()) {
        std::visit(VisitorHelper{
            [&](auto& v) {
                v.Undo();
            }
        }, transformations_.back());
        transformations_.pop_back();
    }
}

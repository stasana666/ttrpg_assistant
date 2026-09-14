#include <deal_damage.h>
#include <pf2e_engine/combat_calculator.h>
#include <pf2e_engine/mechanics/damage.h>
#include <pf2e_engine/transformation/transformator.h>
#include <pf2e_engine/player.h>

#include <stdexcept>

static const TGameObjectId kDamage = TGameObjectIdManager::Instance().Register("damage");
static const TGameObjectId kTarget = TGameObjectIdManager::Instance().Register("target");

void FDealDamage::operator() (std::shared_ptr<TActionContext> ctx) const
{
    auto damage = std::get<std::shared_ptr<TDamage>>(input_.Get(kDamage, ctx));
    auto target = std::get<TPlayer*>(input_.Get(kTarget, ctx));
    if (damage == nullptr || target == nullptr || target->GetCreature() == nullptr) {
        throw std::invalid_argument("damage delivery requires damage and a creature participant");
    }
    if (target->GetCreature()->IsDead()) {
        throw std::invalid_argument("damage target is dead");
    }

    TCombatCalculator calculator;
    ctx->transformator->DealDamageWithContext(
        target,
        calculator.ResolveDamage(*target->GetCreature(), *damage, *ctx->dice_roller),
        damage->Context());
}

#include <deal_damage.h>
#include <pf2e_engine/combat_calculator.h>
#include <pf2e_engine/mechanics/damage.h>
#include <pf2e_engine/transformation/transformator.h>
#include <pf2e_engine/player.h>

static const TGameObjectId kDamage = TGameObjectIdManager::Instance().Register("damage");
static const TGameObjectId kTarget = TGameObjectIdManager::Instance().Register("target");

void FDealDamage::operator() (std::shared_ptr<TActionContext> ctx) const
{
    auto damage = std::get<std::shared_ptr<TDamage>>(input_.Get(kDamage, ctx));
    auto target = std::get<TPlayer*>(input_.Get(kTarget, ctx));

    TCombatCalculator calculator;
    ctx->transformator->DealDamage(
        target,
        calculator.ResolveDamage(*target->GetCreature(), *damage, *ctx->dice_roller));
}

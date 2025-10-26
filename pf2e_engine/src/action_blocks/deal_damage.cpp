#include <deal_damage.h>
#include <pf2e_engine/mechanics/damage.h>
#include <pf2e_engine/mechanics/damage_resolver.h>

static const TGameObjectId kDamage = TGameObjectIdManager::Instance().Register("damage");
static const TGameObjectId kTarget = TGameObjectIdManager::Instance().Register("target");

void FDealDamage::operator() (TActionContext& ctx) const
{
    auto damage = std::get<std::shared_ptr<TDamage>>(input_.Get(kDamage, ctx));
    auto target = std::get<TPlayer*>(input_.Get(kTarget, ctx));

    ctx.transformator->DealDamage(target->creature, target->creature->DamageResolver()(*damage, *ctx.dice_roller));
}

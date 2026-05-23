#include <weapon_damage_roll.h>

#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/expressions/expressions.h>
#include <pf2e_engine/feat.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/mechanics/damage.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/player.h>

#include <memory>

static const TGameObjectId kAttackerId = TGameObjectIdManager::Instance().Register("attacker");
static const TGameObjectId kWeaponId = TGameObjectIdManager::Instance().Register("weapon");

namespace {

const TGameObjectId kDamageBonusId = TGameObjectIdManager::Instance().Register("damage_bonus");

// Borrows an IExpression owned elsewhere (the $damage_bonus TDamage, which
// lives in the action registry and outlives the $damage object).
class TBorrowedExpression : public IExpression {
public:
    explicit TBorrowedExpression(const IExpression* expr)
        : expr_(expr) {}

    int Value(IRandomGenerator& rng) const override {
        return expr_->Value(rng);
    }

private:
    const IExpression* expr_;
};

std::unique_ptr<IExpression> MaybeDouble(std::unique_ptr<IExpression> expr, bool crit)
{
    if (!crit) {
        return expr;
    }
    return std::make_unique<TProductExpression>(
        std::move(expr), std::make_unique<TNumberExpression>(2));
}

// Exposes an empty $damage_bonus accumulator and runs every feat pipeline of
// the attacker on the current context, so feats may contribute into it.
std::shared_ptr<TDamage> GatherDamageBonus(std::shared_ptr<TActionContext> ctx, TPlayer* attacker)
{
    auto bonus = std::make_shared<TDamage>();
    ctx->game_object_registry->Add(kDamageBonusId, bonus);
    for (const auto& feat : attacker->GetCreature()->Feats()) {
        if (!feat->pipeline.empty()) {
            RunSubPipeline(ctx, feat->pipeline.begin()->get());
        }
    }
    return bonus;
}

// Builds weapon damage (base dice + Strength), evaluates the attacker's feats
// and folds the resulting $damage_bonus in. On a critical hit every term --
// weapon dice and bonus dice alike -- is doubled.
void ApplyWeaponDamage(std::shared_ptr<TActionContext> ctx, const TBlockInput& input,
                       TGameObjectId output, bool crit)
{
    TPlayer* player = std::get<TPlayer*>(input.Get(kAttackerId, ctx));
    TWeapon* weapon = std::get<TWeapon*>(input.Get(kWeaponId, ctx));

    std::shared_ptr<TDamage> bonus = GatherDamageBonus(ctx, player);

    auto damage = std::make_shared<TDamage>();

    int str = player->GetCreature()->GetCharacteristic(ECharacteristic::Strength).GetMod();
    auto weapon_expr = std::make_unique<TSumExpression>(
        std::make_unique<TDiceExpression>(weapon->GetBaseDiceSize()),
        std::make_unique<TNumberExpression>(str));
    damage->Add(weapon->GetDamageType(), MaybeDouble(std::move(weapon_expr), crit));

    for (auto [type, expr] : *bonus) {
        damage->Add(type, MaybeDouble(std::make_unique<TBorrowedExpression>(expr), crit));
    }

    ctx->game_object_registry->Add(output, damage);
}

}  // namespace

void FWeaponDamageRoll::operator ()(std::shared_ptr<TActionContext> ctx) const
{
    ApplyWeaponDamage(ctx, input_, output_, /*crit=*/false);
}

void FCritWeaponDamageRoll::operator ()(std::shared_ptr<TActionContext> ctx) const
{
    ApplyWeaponDamage(ctx, input_, output_, /*crit=*/true);
}

#include <weapon_damage_roll.h>

#include <pf2e_engine/expressions/expressions.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/mechanics/damage.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/player.h>

#include <memory>
#include <stdexcept>

static const TGameObjectId kAttackerId = TGameObjectIdManager::Instance().Register("attacker");
static const TGameObjectId kWeaponId = TGameObjectIdManager::Instance().Register("weapon");

namespace {

const TGameObjectId kDamageBonusId = TGameObjectIdManager::Instance().Register("damage_bonus");

const TWeapon* GetWeapon(const TGameObjectPtr& object)
{
    if (const auto* weapon = std::get_if<TWeapon*>(&object)) {
        return *weapon;
    }
    if (const auto* weapon = std::get_if<const TWeapon*>(&object)) {
        return *weapon;
    }
    throw std::logic_error("expected weapon");
}

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

void ApplyWeaponDamage(std::shared_ptr<TActionContext> ctx, const TBlockInput& input,
                       TGameObjectId output, bool crit)
{
    TPlayer* player = std::get<TPlayer*>(input.Get(kAttackerId, ctx));
    const TWeapon* weapon = GetWeapon(input.Get(kWeaponId, ctx));

    auto damage = std::make_shared<TDamage>();

    int str = player->GetCreature()->GetCharacteristic(ECharacteristic::Strength).Modifier();
    auto weapon_expr = std::make_unique<TSumExpression>(
        std::make_unique<TDiceExpression>(weapon->BaseDiceSize()),
        std::make_unique<TNumberExpression>(str));
    damage->Add(weapon->DamageType(), MaybeDouble(std::move(weapon_expr), crit));

    if (ctx->game_object_registry->Contains(kDamageBonusId)) {
        auto bonus = std::get<std::shared_ptr<TDamage>>(
            ctx->game_object_registry->GetGameObjectPtr(kDamageBonusId));
        for (auto [type, expr] : *bonus) {
            damage->Add(type, MaybeDouble(std::make_unique<TBorrowedExpression>(expr), crit));
        }
    }

    ctx->game_object_registry->Add(output, damage);
}

}

void FWeaponDamageRoll::operator ()(std::shared_ptr<TActionContext> ctx) const
{
    ApplyWeaponDamage(ctx, input_, output_, false);
}

void FCritWeaponDamageRoll::operator ()(std::shared_ptr<TActionContext> ctx) const
{
    ApplyWeaponDamage(ctx, input_, output_, true);
}

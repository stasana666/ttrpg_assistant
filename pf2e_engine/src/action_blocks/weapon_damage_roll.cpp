#include <weapon_damage_roll.h>

#include <pf2e_engine/expressions/expressions.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <memory>

static const TGameObjectId kAttackerId = TGameObjectIdManager::Instance().Register("attacker");
static const TGameObjectId kWeaponId = TGameObjectIdManager::Instance().Register("weapon");

void FWeaponDamageRoll::operator ()(TActionContext& ctx) const
{
    TCreature* creature = std::get<TCreature*>(input_.Get(kAttackerId, ctx));
    TWeapon& weapon = ctx.game_object_register.Get<TWeapon>(kWeaponId);

    auto damage = std::make_shared<TDamage>();

    auto dice_expr = std::make_unique<TDiceExpression>(weapon.GetBaseDiceSize());

    int str = creature->GetCharacteristic(ECharacteristic::Strength).GetMod();
    auto str_expr = std::make_unique<TNumberExpression>(str);

    damage->Add(weapon.GetDamageType(), std::make_unique<TSumExpression>(
        std::move(dice_expr),
        std::move(str_expr)
    ));

    ctx.game_object_register.Add(output_, damage);
}

void FCritWeaponDamageRoll::operator ()(TActionContext& ctx) const
{
    TCreature* creature = std::get<TCreature*>(input_.Get(kAttackerId, ctx));
    TWeapon& weapon = ctx.game_object_register.Get<TWeapon>(kWeaponId);

    auto damage = std::make_shared<TDamage>();

    auto dice_expr = std::make_unique<TDiceExpression>(weapon.GetBaseDiceSize());

    int str = creature->GetCharacteristic(ECharacteristic::Strength).GetMod();
    auto str_expr = std::make_unique<TNumberExpression>(str);

    damage->Add(weapon.GetDamageType(), std::make_unique<TProductExpression>(
        std::make_unique<TSumExpression>(
            std::move(dice_expr),
            std::move(str_expr)
        ),
        std::make_unique<TNumberExpression>(2)
    ));

    ctx.game_object_register.Add(output_, damage);
}

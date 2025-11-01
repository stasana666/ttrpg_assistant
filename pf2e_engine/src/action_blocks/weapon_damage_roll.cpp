#include <weapon_damage_roll.h>

#include <pf2e_engine/expressions/expressions.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/player.h>

#include <memory>

static const TGameObjectId kAttackerId = TGameObjectIdManager::Instance().Register("attacker");
static const TGameObjectId kWeaponId = TGameObjectIdManager::Instance().Register("weapon");

void FWeaponDamageRoll::operator ()(TActionContext& ctx) const
{
    TPlayer* player = std::get<TPlayer*>(input_.Get(kAttackerId, ctx));
    TWeapon* weapon = std::get<TWeapon*>(input_.Get(kWeaponId, ctx));

    auto damage = std::make_shared<TDamage>();

    auto dice_expr = std::make_unique<TDiceExpression>(weapon->GetBaseDiceSize());

    int str = player->creature->GetCharacteristic(ECharacteristic::Strength).GetMod();
    auto str_expr = std::make_unique<TNumberExpression>(str);

    damage->Add(weapon->GetDamageType(), std::make_unique<TSumExpression>(
        std::move(dice_expr),
        std::move(str_expr)
    ));

    ctx.game_object_registry->Add(output_, damage);
}

void FCritWeaponDamageRoll::operator ()(TActionContext& ctx) const
{
    TPlayer* player = std::get<TPlayer*>(input_.Get(kAttackerId, ctx));
    TWeapon* weapon = std::get<TWeapon*>(input_.Get(kWeaponId, ctx));

    auto damage = std::make_shared<TDamage>();

    auto dice_expr = std::make_unique<TDiceExpression>(weapon->GetBaseDiceSize());

    int str = player->creature->GetCharacteristic(ECharacteristic::Strength).GetMod();
    auto str_expr = std::make_unique<TNumberExpression>(str);

    damage->Add(weapon->GetDamageType(), std::make_unique<TProductExpression>(
        std::make_unique<TSumExpression>(
            std::move(dice_expr),
            std::move(str_expr)
        ),
        std::make_unique<TNumberExpression>(2)
    ));

    ctx.game_object_registry->Add(output_, damage);
}

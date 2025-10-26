#include <attack_roll.h>
#include <pf2e_engine/success_level.h>
#include <pf2e_engine/player.h>

static const TGameObjectId kAttackerId = TGameObjectIdManager::Instance().Register("attacker");
static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("target");
static const TGameObjectId kWeaponId = TGameObjectIdManager::Instance().Register("weapon");

void FAttackRoll::operator() (TActionContext& ctx) const
{
    TPlayer& attacker = ctx.game_object_register.Get<TPlayer>(kAttackerId);
    TPlayer& target = ctx.game_object_register.Get<TPlayer>(kTargetId);
    TWeapon& weapon = ctx.game_object_register.Get<TWeapon>(kWeaponId);

    int armor_class = calculator_.ArmorClass(*target.creature);
    int attack_bonus = calculator_.AttackRollBonus(*attacker.creature, weapon);

    ESuccessLevel result = calculator_.RollD20(ctx.dice_roller, attack_bonus, armor_class);
    ctx.game_object_register.Add(output_, result);
}

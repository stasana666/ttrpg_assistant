#include <attack_roll.h>
#include "success_level.h"

static const TGameObjectId kAttackerId = TGameObjectIdManager::Instance().Register("attacker");
static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("target");
static const TGameObjectId kWeaponId = TGameObjectIdManager::Instance().Register("weapon");

void FAttackRoll::operator() (TActionContext& ctx) const
{
    TCreature& attacker = ctx.game_object_register.Get<TCreature>(kAttackerId);
    TCreature& target = ctx.game_object_register.Get<TCreature>(kTargetId);
    TWeapon& weapon = ctx.game_object_register.Get<TWeapon>(kWeaponId);

    int armor_class = calculator_.ArmorClass(target);
    int attack_bonus = calculator_.AttackRollBonus(attacker, weapon);

    ESuccessLevel result = calculator_.RollD20(ctx.dice_roller, attack_bonus, armor_class);
    ctx.game_object_register.Add(output_, result);
}

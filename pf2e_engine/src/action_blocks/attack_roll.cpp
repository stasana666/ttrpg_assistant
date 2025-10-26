#include <attack_roll.h>

#include <pf2e_engine/success_level.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include "weapon.h"

static const TGameObjectId kAttackerId = TGameObjectIdManager::Instance().Register("attacker");
static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("target");
static const TGameObjectId kWeaponId = TGameObjectIdManager::Instance().Register("weapon");

void FAttackRoll::operator() (TActionContext& ctx) const
{
    TPlayer& attacker = *std::get<TPlayer*>(input_.Get(kAttackerId, ctx));
    TPlayer& target = *std::get<TPlayer*>(input_.Get(kTargetId, ctx));
    TWeapon& weapon = *std::get<TWeapon*>(input_.Get(kWeaponId, ctx));

    int armor_class = calculator_.ArmorClass(*target.creature);
    int attack_bonus = calculator_.AttackRollBonus(*attacker.creature, weapon);

    ESuccessLevel result = calculator_.RollD20(ctx.dice_roller, attack_bonus, armor_class);
    std::cerr << TGameObjectIdManager::Instance().Name(output_) << std::endl;
    ctx.game_object_registry->Add(output_, result);
}

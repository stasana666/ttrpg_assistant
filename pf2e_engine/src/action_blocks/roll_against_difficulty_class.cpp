#include <roll_against_difficulty_class.h>

#include <pf2e_engine/success_level.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/interaction_system.h>
#include <pf2e_engine/common/errors.h>

static const TGameObjectId kTypeId = TGameObjectIdManager::Instance().Register("type");
static const TGameObjectId kDifficultyClassValueId = TGameObjectIdManager::Instance().Register("DC");
static const TGameObjectId kAttackerId = TGameObjectIdManager::Instance().Register("attacker");
static const TGameObjectId kWeaponId = TGameObjectIdManager::Instance().Register("weapon");

enum class EAttackType {
    Skill,
    WeaponAttack,
};

EAttackType AttackTypeFromString(const std::string& s) {
    if (s == "skill") {
        return EAttackType::Skill;
    }
    if (s == "weapon_attack") {
        return EAttackType::WeaponAttack;
    }
    throw std::invalid_argument("unknown attack type: " + s);
}

void FRollAgainstDifficultyClass::operator() (TActionContext& ctx) const
{
    switch (AttackTypeFromString(input_.GetString(kTypeId))) {
        case EAttackType::Skill:
            return SkillHandle(ctx);
        case EAttackType::WeaponAttack:
            return WeaponAttackHandle(ctx);
    }
    throw std::invalid_argument("invalid DC type: FCalculateDifficultyClass::operator()");
}

void FRollAgainstDifficultyClass::WeaponAttackHandle(TActionContext& ctx) const
{
    TPlayer& attacker = *std::get<TPlayer*>(input_.Get(kAttackerId, ctx));
    TWeapon& weapon = *std::get<TWeapon*>(input_.Get(kWeaponId, ctx));
    int armor_class = std::get<int>(input_.Get(kDifficultyClassValueId, ctx));
    int attack_bonus = calculator_.AttackRollBonus(*attacker.creature, weapon);

    ctx.io_system->GameLog() << attacker.name << " attack with " << weapon.Name() << std::endl;

    ESuccessLevel result = calculator_.RollD20(ctx.dice_roller, attack_bonus, armor_class);
    ctx.io_system->GameLog() << "d20 + " << attack_bonus << " => " << ToString(result) << std::endl;

    ctx.game_object_registry->Add(output_, result);
}

void FRollAgainstDifficultyClass::SkillHandle(TActionContext&) const
{
    throw ToDoError("FRollAgainstDifficultyClass::SkillHandle(TActionContext& ctx)");
}

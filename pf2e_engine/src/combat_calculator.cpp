#include <pf2e_engine/combat_calculator.h>

#include <pf2e_engine/condition.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/mechanics/savethrows.h>
#include <pf2e_engine/success_level.h>

struct TWeaponAttackTag {};
struct TArmorClassTag {};

int TCombatCalculator::InitiativeBonus(const TCreature& creature) const
{
    return creature.GetCharacteristic(ECharacteristic::Dexterity).GetMod();
}

int TCombatCalculator::ArmorClass(const TCreature& target, const TCreature& attacker) const
{
    const TArmor& armor = target.Armor();
    int dex = target.GetCharacteristic(ECharacteristic::Dexterity).GetMod();
    int ac = 10 + armor.ArmorClassBonus()
        + std::min(armor.DexterityCap(), dex)
        + target.Proficiency().GetProficiency(armor)
        - Penalty(target, TArmorClassTag{});
    if (IsOffGuardFor(target, attacker)) {
        ac -= 2;
    }
    return ac;
}

bool TCombatCalculator::IsOffGuardFor(const TCreature& target, [[maybe_unused]] const TCreature& attacker) const
{
    return target.Get(ECondition::Prone) > 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <class T>
int TCombatCalculator::RollBonus(const TCreature& creature, T type) const
{
    int proficiency    = creature.Proficiency().GetProficiency(type);
    int characteristic = creature.GetCharacteristic(BindedCharacteristic(type)).GetMod();
    return proficiency + characteristic - Penalty(creature, type);
}

template int TCombatCalculator::RollBonus<ESkill>(const TCreature&, ESkill) const;
template int TCombatCalculator::RollBonus<ESavethrow>(const TCreature&, ESavethrow) const;
template int TCombatCalculator::RollBonus<TPerceptionTag>(const TCreature&, TPerceptionTag) const;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <class T>
int TCombatCalculator::DifficultyClass(const TCreature& creature, T type) const
{
    return 10 + RollBonus(creature, type);
}

template int TCombatCalculator::DifficultyClass<ESkill>(const TCreature&, ESkill) const;
template int TCombatCalculator::DifficultyClass<ESavethrow>(const TCreature&, ESavethrow) const;
template int TCombatCalculator::DifficultyClass<TPerceptionTag>(const TCreature&, TPerceptionTag) const;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <class T>
int TCombatCalculator::Penalty(const TCreature& creature, T) const
{
    if constexpr (std::is_same_v<T, TWeaponAttackTag>) {
        return creature.Get(ECondition::Frightened)
            + (creature.Get(ECondition::Prone) ? 2 : 0);
    }
    return creature.Get(ECondition::Frightened);
}

template int TCombatCalculator::Penalty<ESkill>(const TCreature&, ESkill) const;
template int TCombatCalculator::Penalty<ESavethrow>(const TCreature&, ESavethrow) const;
template int TCombatCalculator::Penalty<TPerceptionTag>(const TCreature&, TPerceptionTag) const;
template int TCombatCalculator::Penalty<TArmorClassTag>(const TCreature&, TArmorClassTag) const;
template int TCombatCalculator::Penalty<TWeaponAttackTag>(const TCreature&, TWeaponAttackTag) const;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int TCombatCalculator::AttackRollBonus(const TCreature& creature, const TWeapon& weapon) const
{
    int str = creature.GetCharacteristic(ECharacteristic::Strength).GetMod();
    int ability_mod = str;

    // Finesse weapons use the higher of Strength or Dexterity
    if (weapon.HasTrait(EWeaponTrait::Finesse)) {
        int dex = creature.GetCharacteristic(ECharacteristic::Dexterity).GetMod();
        ability_mod = std::max(str, dex);
    }

    return ability_mod
        + creature.Proficiency().GetProficiency(weapon)
        - Penalty(creature, TWeaponAttackTag{});
}

ESuccessLevel TCombatCalculator::RollD20(IRandomGenerator* roller, int bonus, int ac) const
{
    int roll = roller->RollDice(20);
    int roll_result = bonus + roll;
    ESuccessLevel result = [](int diff) {
        if (diff >= 10) {
            return ESuccessLevel::CriticalSuccess;
        }
        if (diff >= 0) {
            return ESuccessLevel::Success;
        }
        if (diff > -10) { // странная антисимметрия в правилах pf2e
            return ESuccessLevel::Failure;
        }
        return ESuccessLevel::CriticalFailure;
    }(roll_result - ac);
    if (roll == 20) {
        result = IncreaseSuccessLevel(result);
    }
    if (roll == 1) {
        result = DecreaseSuccessLevel(result);
    }
    return result;
}

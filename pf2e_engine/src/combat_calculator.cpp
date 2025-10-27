#include <pf2e_engine/combat_calculator.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/success_level.h>

int TCombatCalculator::InitiativeBonus(const TCreature& creature) const
{
    return creature.GetCharacteristic(ECharacteristic::Dexterity).GetMod();
}

int TCombatCalculator::ArmorClass(const TCreature& creature) const
{
    const TArmor& armor = creature.Armor();
    int dex = creature.GetCharacteristic(ECharacteristic::Dexterity).GetMod();
    return 10 + armor.AcBonus() + std::min(armor.DexCap(), dex) + creature.Proficiency().GetProficiency(armor);
}

int TCombatCalculator::AttackRollBonus(const TCreature& creature, const TWeapon& weapon) const
{
    int str = creature.GetCharacteristic(ECharacteristic::Strength).GetMod();
    return str + creature.Proficiency().GetProficiency(weapon);
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

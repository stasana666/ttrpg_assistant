#include <pf2e_engine/combat_calculator.h>
#include "success_level.h"

int TCombatCalculator::ArmorClass(const TCreature& creature) const
{
    const TArmor& armor = creature.Armor();
    int dex = creature.GetCharacteristic(ECharacteristic::Dexterity).GetMod();
    return 10 + armor.AcBonus() + std::min(armor.DexCap(), dex);
}

int TCombatCalculator::AttackRollBonus(const TCreature& creature, const TWeapon&) const
{
    int str = creature.GetCharacteristic(ECharacteristic::Strength).GetMod();
    return str;
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

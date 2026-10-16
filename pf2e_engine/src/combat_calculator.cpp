#include <pf2e_engine/combat_calculator.h>

#include <pf2e_engine/common/rule_integer.h>
#include <pf2e_engine/expressions/base_expression.h>
#include <pf2e_engine/inventory/combat_checks.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/mechanics/damage.h>
#include <pf2e_engine/mechanics/savethrows.h>
#include <pf2e_engine/success_level.h>

#include <stdexcept>
#include <type_traits>


int TCombatCalculator::InitiativeBonus(const TCreature& creature) const
{
    return creature.InitiativeBonus();
}

int TCombatCalculator::InitiativeBonus(const TCreature& creature, const TCheckModifiers& modifiers) const
{
    return creature.InitiativeBonusWithModifiers(modifiers);
}

int TCombatCalculator::ArmorClass(
    const TCreature& target, [[maybe_unused]] const TCreature& attacker) const
{
    return target.ArmorClass();
}

int TCombatCalculator::ResolveDamage(
    const TCreature& target,
    const TDamage& damage,
    IRandomGenerator& rng) const
{
    const auto rules = TCheckRules::Create({});
    int result = 0;
    for (const auto& [type, expression] : damage) {
        const int amount = rules.MinimumDamage(expression->Value(rng));
        result = TRuleInteger::Add(result, target.DamageAmount(type, amount));
    }
    return result;
}


template <class T>
int TCombatCalculator::RollBonus(const TCreature& creature, T type) const
{
    if constexpr (std::is_same_v<T, ESkill>) {
        return creature.SkillBonus(type);
    } else if constexpr (std::is_same_v<T, ESavethrow>) {
        return creature.SaveBonus(type);
    } else {
        static_assert(std::is_same_v<T, TPerceptionTag>);
        return creature.PerceptionBonus();
    }
}

template int TCombatCalculator::RollBonus<ESkill>(const TCreature&, ESkill) const;
template int TCombatCalculator::RollBonus<ESavethrow>(const TCreature&, ESavethrow) const;
template int TCombatCalculator::RollBonus<TPerceptionTag>(const TCreature&, TPerceptionTag) const;


template <class T>
int TCombatCalculator::DifficultyClass(const TCreature& creature, T type) const
{
    if constexpr (std::is_same_v<T, ESkill>) {
        return creature.SkillDC(type);
    } else if constexpr (std::is_same_v<T, ESavethrow>) {
        return creature.SaveDC(type);
    } else {
        static_assert(std::is_same_v<T, TPerceptionTag>);
        return creature.PerceptionDC();
    }
}

template int TCombatCalculator::DifficultyClass<ESkill>(const TCreature&, ESkill) const;
template int TCombatCalculator::DifficultyClass<ESavethrow>(const TCreature&, ESavethrow) const;
template int TCombatCalculator::DifficultyClass<TPerceptionTag>(const TCreature&, TPerceptionTag) const;


template <class T>
int TCombatCalculator::Penalty(const TCreature& creature, T) const
{
    return creature.ConditionPenalty();
}

template int TCombatCalculator::Penalty<ESkill>(const TCreature&, ESkill) const;
template int TCombatCalculator::Penalty<ESavethrow>(const TCreature&, ESavethrow) const;
template int TCombatCalculator::Penalty<TPerceptionTag>(const TCreature&, TPerceptionTag) const;


int TCombatCalculator::AttackRollBonus(const TCreature& creature, const TWeapon& weapon) const
{
    return creature.AttackBonus(weapon);
}

ESuccessLevel TCombatCalculator::RollD20(IRandomGenerator* roller, int bonus, int ac) const
{
    const int roll = roller->RollDice(20);
    switch (TCheckRules::Create({}).Resolve(roll, bonus, ac)) {
        case ECheckOutcome::CriticalFailure:
            return ESuccessLevel::CriticalFailure;
        case ECheckOutcome::Failure:
            return ESuccessLevel::Failure;
        case ECheckOutcome::Success:
            return ESuccessLevel::Success;
        case ECheckOutcome::CriticalSuccess:
            return ESuccessLevel::CriticalSuccess;
    }
    throw std::logic_error("invalid generated check outcome");
}

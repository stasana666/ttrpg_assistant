#include <pf2e_engine/random.h>

#include <algorithm>
#include <cassert>
#include "game_context.h"
#include <damage_resolver.h>

void TDamageResolver::AddImmunity(TDamage::Type type)
{
    immunities_.insert(type);
}

void TDamageResolver::AddResistance(TDamage::Type type, int value)
{
    resistances_[type].push_back(value);
}

void TDamageResolver::AddVulnerability(TDamage::Type type, int value)
{
    vulnerabilities_[type].push_back(value);
}

/*
TODO(stasana):
Сейчас обсчет урона кажется неправильный. Проблема возникает из-за неполной информации в правилах настолки
Нужно уточнить как именно на урон влияют штрафы и бонусы. Правда ли в системе разделяются понятия Damage и DamageRoll
После получения информации логика урона может быть полностью переписана.
Ввиду ненужности на текущем этапе такой сложной схемы подсчета урона пока оставляю так
*/
int TDamageResolver::operator()(const TDamage& damage, IRandomGenerator& rng) const
{
    TGameContext ctx{.game_object_register = nullptr, .dice_roller = &rng};
    int result = 0;
    for (const auto& [type, expr] : damage) {
        int value = std::max(1, expr->Value(ctx));
        if (immunities_.contains(type)) {
            continue;
        }
        if (auto it = vulnerabilities_.find(type); it != vulnerabilities_.end()) {
            auto& vec = it->second;
            assert(!vec.empty());
            value += *std::max_element(vec.begin(), vec.end());
        }
        if (auto it = resistances_.find(type); it != resistances_.end()) {
            auto& vec = it->second;
            assert(!vec.empty());
            value -= *std::max_element(vec.begin(), vec.end());
        }
        result += std::max(0, value);
    }
    return result;
}

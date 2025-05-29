#include <pf2e_engine/random.h>

#include <algorithm>
#include <cassert>
#include "game_context.h"
#include <damage_resolver.h>

void TDamageResolver::AddImmunity(TDamage::Type type)
{
    immunities.insert(type);
}

void TDamageResolver::AddResistance(TDamage::Type type, int value)
{
    resistances[type].push_back(value);
}

void TDamageResolver::AddVulnerability(TDamage::Type type, int value)
{
    vulnerabilities[type].push_back(value);
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
    TGameContext ctx{.gameObjectStorage = nullptr, .diceRoller = &rng};
    int result = 0;
    for (const auto& [type, expr] : damage) {
        int value = std::max(1, expr->Value(ctx));
        if (immunities.contains(type)) {
            continue;
        }
        if (auto it = vulnerabilities.find(type); it != vulnerabilities.end()) {
            auto& vec = it->second;
            assert(!vec.empty());
            value += *std::max_element(vec.begin(), vec.end());
        }
        if (auto it = resistances.find(type); it != resistances.end()) {
            auto& vec = it->second;
            assert(!vec.empty());
            value -= *std::max_element(vec.begin(), vec.end());
        }
        result += std::max(0, value);
    }
    return result;
}

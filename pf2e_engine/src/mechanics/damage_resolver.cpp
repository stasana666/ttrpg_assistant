#include <pf2e_engine/random.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

#include <algorithm>
#include <cassert>
#include <damage_resolver.h>
#include <utility>

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
    int result = 0;
    for (const auto& [type, expr] : damage) {
        int value = std::max(1, expr->Value(rng));
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

namespace {

TAstNode SerializeTypeMap(
    const std::unordered_map<TDamage::Type, std::vector<int>>& m,
    std::string_view label)
{
    std::vector<std::pair<TDamage::Type, std::vector<int>>> sorted(
        m.begin(), m.end());
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    TAstNode node = TAstNode::MakeObject(std::string(label));
    for (const auto& [type, vec] : sorted) {
        std::vector<int> copy = vec;
        std::sort(copy.begin(), copy.end());
        AddValueField(node, ToString(type), copy);
    }
    return node;
}

}  // namespace

TAstNode TDamageResolver::GetAst([[maybe_unused]] TAstContext& ctx) const
{
    static constexpr size_t kExpectedSize = 176;
    static constexpr size_t kExpectedSentinelOffset = 168;
    AST_ASSERT_LAYOUT_WITH_SENTINEL(TDamageResolver, kExpectedSize, kExpectedSentinelOffset);

    std::vector<TDamage::Type> sorted_immunities(
        immunities_.begin(), immunities_.end());
    std::sort(sorted_immunities.begin(), sorted_immunities.end());

    TAstNode node = TAstNode::MakeObject("TDamageResolver");
    AddValueField(node, "immunities", sorted_immunities);
    node.AddChild("resistances", SerializeTypeMap(resistances_, "map"));
    node.AddChild("vulnerabilities", SerializeTypeMap(vulnerabilities_, "map"));
    return node;
}

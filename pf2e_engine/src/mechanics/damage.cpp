#include "damage.h"

#include <pf2e_engine/expressions/math_expression.h>

void TDamage::Add(Type type, std::unique_ptr<IExpression>&& damage)
{
    auto it = damage_expressions_.find(type);
    if (it != damage_expressions_.end()) {
        it->second = std::make_unique<TSumExpression>(std::move(it->second), std::move(damage));
    } else {
        damage_expressions_[type] = std::move(damage);
    }
}

TDamage::TIterator::TIterator(Container::const_iterator it)
    : it(it)
{
}

auto TDamage::TIterator::operator*() const -> std::pair<Type, const IExpression*>
{
    return std::make_pair(it->first, it->second.get());
}

auto TDamage::TIterator::operator++() -> TIterator&
{
    ++it;
    return *this;
}

bool TDamage::TIterator::operator!=(const TIterator& other) const
{
    return it != other.it;
}

auto TDamage::begin() const -> TIterator
{
    return TIterator(damage_expressions_.cbegin());
}

auto TDamage::end() const -> TIterator
{
    return TIterator(damage_expressions_.end());
}

TDamage::Type DamageTypeFromString(std::string_view sv)
{
    if (sv == "Bludgeoning")
    {
        return TDamage::Type::Bludgeoning;
    }
    if (sv == "Piercing")
    {
        return TDamage::Type::Piercing;
    }
    if (sv == "Slashing")
    {
        return TDamage::Type::Slashing;
    }
    throw std::logic_error("unknown damage type");
}

std::string ToString(TDamage::Type type)
{
    switch (type) {
        case TDamage::Type::Bludgeoning: return "Bludgeoning";
        case TDamage::Type::Piercing:    return "Piercing";
        case TDamage::Type::Slashing:    return "Slashing";
    }
    throw std::runtime_error("invalid damage type");
}

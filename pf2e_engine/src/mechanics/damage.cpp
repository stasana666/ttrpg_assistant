#include "damage.h"

#include <pf2e_engine/expressions/math_expression.h>

void TDamage::Add(std::unique_ptr<IExpression>&& damage, Type type)
{
    auto it = damageExpressions.find(type);
    if (it != damageExpressions.end()) {
        it->second = std::make_unique<TSumExpression>(std::move(it->second), std::move(damage));
    } else {
        damageExpressions[type] = std::move(damage);
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
    return TIterator(damageExpressions.cbegin());
}

auto TDamage::end() const -> TIterator
{
    return TIterator(damageExpressions.end());
}

#include "damage.h"

void TDamage::Add(std::unique_ptr<IExpression>&& damage, Type type)
{
    auto it = damageExpressions.find(type);
    if (it != damageExpressions.end()) {
        it->second = std::make_unique<TSumExpression>(std::move(it->second), std::move(damage));
    } else {
        damageExpressions[type] = std::move(damage);
    }
}

TDamage::Iterator::Iterator(Container::const_iterator it)
    : it(it)
{
}

auto TDamage::Iterator::operator*() const -> std::pair<Type, const IExpression*>
{
    return std::make_pair(it->first, it->second.get());
}

auto TDamage::Iterator::operator++() -> Iterator&
{
    ++it;
    return *this;
}

bool TDamage::Iterator::operator!=(const Iterator& other) const
{
    return it != other.it;
}

auto TDamage::begin() const -> Iterator
{
    return Iterator(damageExpressions.cbegin());
}

auto TDamage::end() const -> Iterator
{
    return Iterator(damageExpressions.end());
}

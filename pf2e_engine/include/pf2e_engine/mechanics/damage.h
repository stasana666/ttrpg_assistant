#pragma once

#include <pf2e_engine/expressions/base_expression.h>

#include <memory>
#include <unordered_map>

class TDamage {
public:
    enum class Type {
        Bludgeoning,
        Piercing,
        Slashing,
    };

private:
    using Container = std::unordered_map<Type, std::unique_ptr<IExpression>>;

public:
    void Add(std::unique_ptr<IExpression>&& damage, Type type);

    class TIterator {
    public:
        bool operator !=(const TIterator&) const;
        TIterator& operator++();
        std::pair<Type, const IExpression*> operator *() const;
    
    private:
        friend TDamage;

        explicit TIterator(Container::const_iterator);

        typename Container::const_iterator it;
    };

    TIterator begin() const;
    TIterator end() const;

private:
    
    Container damageExpressions;
};

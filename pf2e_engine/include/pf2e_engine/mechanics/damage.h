#pragma once

#include <pf2e_engine/dice_expression/dice.h>
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

    class Iterator {
    public:
        bool operator !=(const Iterator&) const;
        Iterator& operator++();
        std::pair<Type, const IExpression*> operator *() const;
    
    private:
        friend TDamage;

        Iterator(Container::const_iterator);

        typename Container::const_iterator it;
    };

    Iterator begin() const;
    Iterator end() const;

private:
    
    Container damageExpressions;
};

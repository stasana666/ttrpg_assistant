#pragma once

#include <pf2e_engine/expressions/base_expression.h>
#include <pf2e_engine/inventory/damage.h>  // generated; defines EDamageType

#include <memory>
#include <unordered_map>

class TDamage {
private:
    using Container = std::unordered_map<EDamageType, std::unique_ptr<IExpression>>;

public:
    void Add(EDamageType type, std::unique_ptr<IExpression>&& damage);

    class TIterator {
    public:
        bool operator !=(const TIterator&) const;
        TIterator& operator++();
        std::pair<EDamageType, const IExpression*> operator *() const;

    private:
        friend TDamage;

        explicit TIterator(Container::const_iterator);

        typename Container::const_iterator it;
    };

    TIterator begin() const;
    TIterator end() const;

private:

    Container damage_expressions_;
};

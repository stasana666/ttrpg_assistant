#pragma once

#include "dice.h"
#include <array>
#include <memory>

class TDamage {
public:
    enum class Type {
        Bludgeoning,
        Piercing,
        Slashing,
        COUNT,
    };

    static constexpr int kDamageTypeCount = static_cast<int>(Type::COUNT);

private:
    std::array<std::unique_ptr<IExpression>, kDamageTypeCount> damageExpressions;
};

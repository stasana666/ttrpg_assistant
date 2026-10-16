#pragma once

#include <limits>
#include <stdexcept>

struct TRuleInteger {
    static int Checked(long long value)
    {
        if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
            throw std::overflow_error("rule integer overflow");
        }
        return static_cast<int>(value);
    }

    static int Add(int left, int right) { return Checked(static_cast<long long>(left) + right); }
    static int Subtract(int left, int right) { return Checked(static_cast<long long>(left) - right); }
    static int Multiply(int left, int right) { return Checked(static_cast<long long>(left) * right); }
    static int Negate(int value) { return Checked(-static_cast<long long>(value)); }

    static int Divide(int left, int right)
    {
        if (right == 0) throw std::domain_error("rule division by zero");
        return Checked(static_cast<long long>(left) / right);
    }
};

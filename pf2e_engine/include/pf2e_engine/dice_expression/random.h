#pragma once

#include <random>

class IRandomGenerator {
public:
    virtual int RollDice(int size) = 0;
    virtual ~IRandomGenerator() = default;
};

class TRandomGenerator final : public IRandomGenerator {
public:
    explicit TRandomGenerator(int seed);
    int RollDice(int size) final;

private:
    std::mt19937 rng;
};

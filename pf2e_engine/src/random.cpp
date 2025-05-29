#include "random.h"
#include <random>

TRandomGenerator::TRandomGenerator(int seed)
    : rng(seed)
{
}

int TRandomGenerator::RollDice(int size)
{
    return std::uniform_int_distribution<int>(1, size)(rng);
}

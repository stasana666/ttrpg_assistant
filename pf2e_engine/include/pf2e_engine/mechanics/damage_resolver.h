#pragma once

#include "damage.h"

#include <unordered_set>
#include <unordered_map>
#include <vector>

class TDamageResolver {
public:
    void AddImmunity(TDamage::Type);
    void AddResistance(TDamage::Type type, int value);
    void AddVulnerability(TDamage::Type type, int value);

    int operator()(const TDamage&, IRandomGenerator&) const;

private:
    std::unordered_set<TDamage::Type> immunities;
    std::unordered_map<TDamage::Type, std::vector<int>> resistances;
    std::unordered_map<TDamage::Type, std::vector<int>> vulnerabilities;
};

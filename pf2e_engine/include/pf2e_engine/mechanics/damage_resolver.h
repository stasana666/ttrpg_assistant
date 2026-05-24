#pragma once

#include "damage.h"
#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/random.h>

#include <unordered_set>
#include <unordered_map>
#include <vector>

class TDamageResolver {
public:
    void AddImmunity(TDamage::Type);
    void AddResistance(TDamage::Type type, int value);
    void AddVulnerability(TDamage::Type type, int value);

    int operator()(const TDamage&, IRandomGenerator&) const;

    TAstNode GetAst(TAstContext& ctx) const;

private:
    std::unordered_set<TDamage::Type> immunities_;
    std::unordered_map<TDamage::Type, std::vector<int>> resistances_;
    std::unordered_map<TDamage::Type, std::vector<int>> vulnerabilities_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TDamageResolver> : std::true_type {};

#pragma once

#include <pf2e_engine/condition.h>
#include <pf2e_engine/common/hash_combine.h>

#include <functional>
#include <variant>
#include <set>

class TPlayer;

struct TPlayerConditionSet {
    TPlayer* player;
    ECondition condition;
    int value;

    using Key = std::pair<TPlayer*, ECondition>;
};

using TEffect = std::variant<
    TPlayerConditionSet
>;

using TEffectCanceler = std::function<void()>;

class TEffectManager {
public:
    TEffectCanceler AddEffect(TEffect effect);

private:
    void Update(TPlayer* player, ECondition condition);

    using ConditionKey = std::pair<TPlayer*, ECondition>;

    class FConditionKeyHasher {
    public:
        size_t operator ()(const ConditionKey& key) const {
            return HashCombine(
                hasher_(reinterpret_cast<size_t>(key.first)), 
                hasher_(static_cast<size_t>(key.second))
            );
        }

    private:
        std::hash<size_t> hasher_;
    };

    std::unordered_map<ConditionKey, std::multiset<int>, FConditionKeyHasher> condition_values_;
};

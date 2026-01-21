#pragma once

#include <pf2e_engine/condition.h>
#include <pf2e_engine/common/hash_combine.h>

#include <functional>
#include <variant>
#include <set>

class TPlayer;
class TTransformator;

struct TPlayerConditionSet {
    TPlayer* player;
    ECondition condition;
    int value;
};

using TEffect = std::variant<
    TPlayerConditionSet
>;

enum class EEffectCancelPolicy {
    Cancel,
    ReduceUntilZero,
};

using TEffectCanceler = std::function<bool(EEffectCancelPolicy)>;

class TEffectManager {
public:
    TEffectCanceler AddEffect(TEffect effect, TTransformator& transformator);

    // Methods for transformation undo access
    void InsertValue(TPlayer* player, ECondition condition, int value);
    void EraseValue(TPlayer* player, ECondition condition, int value);

    // Get the current highest value for a player/condition pair (0 if none)
    int GetHighestValue(TPlayer* player, ECondition condition) const;

private:
    void Update(TPlayer* player, ECondition condition, TTransformator& transformator);

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

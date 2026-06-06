#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/hash_combine.h>
#include <pf2e_engine/condition.h>

#include <functional>
#include <variant>
#include <set>

class TPlayer;
class TTransformator;

struct TPlayerConditionSet {
    TPlayer* player;
    EConditionKind condition;
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

    void ClearCondition(TPlayer* player, EConditionKind condition, TTransformator& transformator);

    void InsertValue(TPlayer* player, EConditionKind condition, int value);
    void EraseValue(TPlayer* player, EConditionKind condition, int value);

    int GetHighestValue(TPlayer* player, EConditionKind condition) const;

    TAstNode GetAst(TAstContext& ctx) const;

private:
    void Update(TPlayer* player, EConditionKind condition, TTransformator& transformator);

    using ConditionKey = std::pair<TPlayer*, EConditionKind>;

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
    std::unordered_map<ConditionKey, std::vector<TEffectCanceler>, FConditionKeyHasher> active_cancelers_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TEffectManager> : std::true_type {};

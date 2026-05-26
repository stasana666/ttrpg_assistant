#include <pf2e_engine/effect_manager.h>
#include <pf2e_engine/common/visit.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/condition.h>
#include <pf2e_engine/transformation/transformator.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

#include <algorithm>
#include <vector>

void TEffectManager::InsertValue(TPlayer* player, ECondition condition, int value)
{
    condition_values_[std::make_pair(player, condition)].insert(value);
}

void TEffectManager::EraseValue(TPlayer* player, ECondition condition, int value)
{
    auto it = condition_values_.find(std::make_pair(player, condition));
    if (it != condition_values_.end()) {
        auto value_it = it->second.find(value);
        if (value_it != it->second.end()) {
            it->second.erase(value_it);
        }
    }
}

int TEffectManager::GetHighestValue(TPlayer* player, ECondition condition) const
{
    auto it = condition_values_.find(std::make_pair(player, condition));
    if (it == condition_values_.end() || it->second.empty()) {
        return 0;
    }
    return *it->second.rbegin();
}

TEffectCanceler TEffectManager::AddEffect(TEffect effect, TTransformator& transformator)
{
    TEffectCanceler canceler;
    std::visit(VisitorHelper{
        [&](TPlayerConditionSet effect_pcs) {
            // Record the effect addition via transformation
            transformator.AddEffect(this, effect_pcs.player, effect_pcs.condition, effect_pcs.value);
            // Update the creature's condition via transformation
            Update(effect_pcs.player, effect_pcs.condition, transformator);

            // Shared state: any copy of `canceler` (e.g. one captured into a
            // scheduled task via [canceler]) sees the same `state->value`.
            // Without this, ClearCondition could neutralize the original
            // canceler while a copy in a task still holds value=2 and would
            // resurrect the condition on the next OnTurnStart by re-adding
            // a decremented value.
            auto state = std::make_shared<TPlayerConditionSet>(effect_pcs);

            canceler = [state, this, &transformator](EEffectCancelPolicy ecp) {
                bool need_repeat = false;
                if (state->value > 0) {
                    switch (ecp) {
                        case EEffectCancelPolicy::Cancel:
                            transformator.RemoveEffect(this, state->player, state->condition, state->value);
                            state->value = 0;
                            break;
                        case EEffectCancelPolicy::ReduceUntilZero:
                            transformator.RemoveEffect(this, state->player, state->condition, state->value);
                            --state->value;
                            if (state->value > 0) {
                                transformator.AddEffect(this, state->player, state->condition, state->value);
                                need_repeat = true;
                            }
                            break;
                    }
                    Update(state->player, state->condition, transformator);
                }
                return need_repeat;
            };

            active_cancelers_[std::make_pair(effect_pcs.player, effect_pcs.condition)].push_back(canceler);
        }
    }, effect);

    return canceler;
}

void TEffectManager::ClearCondition(TPlayer* player, ECondition condition, TTransformator& transformator)
{
    auto key = std::make_pair(player, condition);
    auto it = active_cancelers_.find(key);
    if (it == active_cancelers_.end()) {
        // No effect_manager-tracked source — condition was set directly via
        // ChangeCondition (e.g. Prone). Just zero it out.
        transformator.ChangeCondition(player->GetCreature(), condition, 0);
        return;
    }
    // Move cancelers out before invoking so re-entry via task callbacks is safe.
    auto cancelers = std::move(it->second);
    active_cancelers_.erase(it);
    for (auto& canceler : cancelers) {
        canceler(EEffectCancelPolicy::Cancel);
    }
}

void TEffectManager::Update(TPlayer* player, ECondition condition, TTransformator& transformator)
{
    int new_value = GetHighestValue(player, condition);
    transformator.ChangeCondition(player->GetCreature(), condition, new_value);
}

TAstNode TEffectManager::GetAst([[maybe_unused]] TAstContext& ctx) const
{
    // TEffectManager is non-standard-layout (has private FConditionKeyHasher
    // member class). offsetof on the sentinel is UB. sizeof alone here.
    static constexpr size_t kExpectedSize = 136;
    AST_ASSERT_LAYOUT(TEffectManager, kExpectedSize);

    // Sort by (player_id_int, condition_enum_int). Both are build-time-known
    // and stable across runs (unlike pointer addresses or resolved string
    // names which would require the context's identity table). std::stable_sort
    // is used so that any two entries that compare equal — which should not
    // happen since (player, condition) is unique in the map — keep their
    // arrival order rather than producing run-to-run noise.
    using SortKey = std::pair<int, int>;
    auto make_sort_key = [](const ConditionKey& k) -> SortKey {
        const int player_id = k.first == nullptr ? -1 : k.first->GetId();
        return {player_id, static_cast<int>(k.second)};
    };
    auto make_label = [&](const ConditionKey& k) {
        std::string label = k.first == nullptr
            ? std::string("<null>")
            : "player#" + std::to_string(k.first->GetId());
        label += ":";
        label += ToString(k.second);
        return label;
    };

    TAstNode node = TAstNode::MakeObject("TEffectManager");

    // condition_values_: multiset is already sorted, render as a vector of values.
    std::vector<std::pair<SortKey, const ConditionKey*>> values_sorted;
    values_sorted.reserve(condition_values_.size());
    for (const auto& [key, _] : condition_values_) {
        values_sorted.emplace_back(make_sort_key(key), &key);
    }
    std::stable_sort(values_sorted.begin(), values_sorted.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    TAstNode values_node = TAstNode::MakeObject("condition_values");
    for (const auto& [_, kptr] : values_sorted) {
        const auto& mset = condition_values_.at(*kptr);
        std::vector<int> as_vec(mset.begin(), mset.end());
        AddValueField(values_node, make_label(*kptr), as_vec);
    }
    node.AddChild("condition_values", std::move(values_node));

    // active_cancelers_: same sort key, same stability guarantee.
    std::vector<std::pair<SortKey, const ConditionKey*>> cancelers_sorted;
    cancelers_sorted.reserve(active_cancelers_.size());
    for (const auto& [key, _] : active_cancelers_) {
        cancelers_sorted.emplace_back(make_sort_key(key), &key);
    }
    std::stable_sort(cancelers_sorted.begin(), cancelers_sorted.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    TAstNode cancelers_node = TAstNode::MakeObject("active_cancelers");
    for (const auto& [_, kptr] : cancelers_sorted) {
        const auto& vec = active_cancelers_.at(*kptr);
        TAstNode entry = TAstNode::MakeObject("cancelers");
        AddValueField(entry, "count", vec.size());
        for (size_t i = 0; i < vec.size(); ++i) {
            AddCallbackPlaceholder(entry, std::to_string(i), vec[i]);
        }
        cancelers_node.AddChild(make_label(*kptr), std::move(entry));
    }
    node.AddChild("active_cancelers", std::move(cancelers_node));

    return node;
}

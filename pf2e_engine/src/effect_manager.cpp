#include <pf2e_engine/effect_manager.h>
#include <pf2e_engine/common/visit.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/condition.h>
#include <pf2e_engine/transformation/transformator.h>

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
        [&](TPlayerConditionSet effect) {
            // Record the effect addition via transformation
            transformator.AddEffect(this, effect.player, effect.condition, effect.value);
            // Update the creature's condition via transformation
            Update(effect.player, effect.condition, transformator);

            canceler = [effect, this, &transformator](EEffectCancelPolicy ecp) mutable {
                bool need_repeat = false;
                switch (ecp) {
                    case EEffectCancelPolicy::Cancel:
                        transformator.RemoveEffect(this, effect.player, effect.condition, effect.value);
                        break;
                    case EEffectCancelPolicy::ReduceUntilZero:
                        transformator.RemoveEffect(this, effect.player, effect.condition, effect.value);
                        --effect.value;
                        if (effect.value > 0) {
                            transformator.AddEffect(this, effect.player, effect.condition, effect.value);
                            need_repeat = true;
                        }
                        break;
                }
                Update(effect.player, effect.condition, transformator);
                return need_repeat;
            };
        }
    }, effect);

    return canceler;
}

void TEffectManager::Update(TPlayer* player, ECondition condition, TTransformator& transformator)
{
    int new_value = GetHighestValue(player, condition);
    transformator.ChangeCondition(player->GetCreature(), condition, new_value);
}

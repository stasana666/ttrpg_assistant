#include <pf2e_engine/effect_manager.h>
#include <pf2e_engine/common/visit.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/condition.h>

TEffectCanceler TEffectManager::AddEffect(TEffect effect)
{
    TEffectCanceler canceler;
    std::visit(VisitorHelper{
        [&](TPlayerConditionSet effect) {
            condition_values_[std::make_pair(effect.player, effect.condition)].insert(effect.value);
            canceler = [effect, this](EEffectCancelPolicy ecp) mutable {
                bool need_repeat = false;
                switch (ecp) {
                    case EEffectCancelPolicy::Cancel:
                        condition_values_.at(std::make_pair(effect.player, effect.condition)).erase(effect.value);
                        break;
                    case EEffectCancelPolicy::ReduceUntilZero:
                        condition_values_.at(std::make_pair(effect.player, effect.condition)).erase(effect.value);
                        --effect.value;
                        if (effect.value > 0) {
                            condition_values_.at(std::make_pair(effect.player, effect.condition)).insert(effect.value);
                            need_repeat = true;
                        }
                        break;
                }
                Update(effect.player, effect.condition);
                return need_repeat;
            };
            Update(effect.player, effect.condition);
        }
    }, effect);

    return canceler;
}

void TEffectManager::Update(TPlayer* player, ECondition condition)
{
    auto& values = condition_values_.at(std::make_pair(player, condition));
    if (values.empty()) {
        player->creature->Set(condition, 0);
    } else {
        player->creature->Set(condition, *values.rbegin());
    }
}

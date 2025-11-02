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
            canceler = [&]() {
                condition_values_.at(std::make_pair(effect.player, effect.condition)).erase(effect.value);
                Update(effect.player, effect.condition);
            };
            Update(effect.player, effect.condition);
        }
    }, effect);

    return canceler;
}

void TEffectManager::Update(TPlayer* player, ECondition condition)
{
    auto& values = condition_values_.at(std::make_pair(player, condition));
    player->creature->Set(condition, *values.rbegin());
}

#include <analyzer/aggressive_melee_strategy.h>

#include <pf2e_engine/player.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_source.h>
#include <pf2e_engine/inventory/movement.h>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

int Distance(TPosition first, TPosition second)
{
    return std::max(std::abs(first.x - second.x), std::abs(first.y - second.y));
}

const TPlayer* FindActor(const TBattle* battle, int player_id)
{
    if (battle == nullptr) {
        return nullptr;
    }
    const auto players = battle->GetIfPlayers([player_id](const TPlayer* player) {
        return player->GetId() == player_id;
    });
    return players.empty() ? nullptr : players.front();
}

int EnemyDistance(const TBattle* battle, const TPlayer* actor, TPosition position)
{
    int distance = std::numeric_limits<int>::max();
    if (battle == nullptr || actor == nullptr) {
        return distance;
    }
    for (const auto* enemy : battle->GetIfPlayers([actor](const TPlayer* player) {
        return player->GetTeam() != actor->GetTeam() && player->GetCreature()->CanAct();
    })) {
        distance = std::min(distance, Distance(position, enemy->GetPosition()));
    }
    return distance;
}

size_t ChooseAction(const TAlternatives& alternatives, const TBattle* battle, const TPlayer* actor)
{
    size_t selected = 0;
    int best = -1;
    const bool reachable = actor == nullptr
        || EnemyDistance(battle, actor, actor->GetPosition()) <= actor->GetCreature()->MaxWeaponReach();
    for (size_t i = 0; i < alternatives.Size(); ++i) {
        const auto option = alternatives[i].Get<std::optional<TAbilityOption>>();
        int priority = -1;
        if (!option) {
            priority = 0;
        } else if (option->DefinitionId == "movement.UseStand") {
            priority = 100;
        } else if (option->DefinitionId == "trip.UseKnockdown" && reachable) {
            priority = 90;
        } else if (option->DefinitionId == "shield_actions.RaiseShield" && actor != nullptr
                   && !actor->GetCreature()->Shield().IsRaised()
                   && actor->GetCreature()->Actions().Count() == 1) {
            priority = 80;
        } else if (reachable && (option->DefinitionId == "abilities.AttackWithWeapon"
                   || option->DefinitionId == "abilities.Strike"
                   || option->DefinitionId == "abilities.UseStrike")) {
            priority = 70;
        } else if (!reachable && option->DefinitionId == "movement.UseSuddenCharge") {
            priority = 60;
        } else if (!reachable && option->DefinitionId == "movement.UseStride") {
            priority = 50;
        }
        if (priority > best) {
            best = priority;
            selected = i;
        }
    }
    return selected;
}

size_t ChooseTarget(int player_id, const TAlternatives& alternatives, const TPlayer* actor)
{
    size_t result = alternatives.Size();
    int lowest_hp = std::numeric_limits<int>::max();
    for (size_t i = 0; i < alternatives.Size(); ++i) {
        const auto* target = alternatives[i].Get<TPlayer*>();
        if (target->GetId() == player_id || (actor != nullptr && target->GetTeam() == actor->GetTeam())
            || !target->GetCreature()->CanAct()) {
            continue;
        }
        const int hp = target->GetCreature()->Hitpoints().Current();
        if (hp < lowest_hp) {
            lowest_hp = hp;
            result = i;
        }
    }
    if (result == alternatives.Size()) {
        throw std::logic_error("melee strategy has no conscious enemy target");
    }
    return result;
}

}

void TAggressiveMeleeStrategy::BindBattle(const TBattle& battle)
{
    battle_ = &battle;
}

size_t TAggressiveMeleeStrategy::Decide(int player_id, const TAlternatives& alternatives) const
{
    const auto* actor = FindActor(battle_, player_id);
    const auto kind = alternatives.Kind();
    if (kind == "next action") {
        return ChooseAction(alternatives, battle_, actor);
    }
    if (kind == "target") {
        return ChooseTarget(player_id, alternatives, actor);
    }
    if (kind == "reaction") {
        const bool has_allies = actor != nullptr && !battle_->GetIfPlayers([actor](const TPlayer* player) {
            return player != actor && player->GetTeam() == actor->GetTeam();
        }).empty();
        for (size_t i = 1; i < alternatives.Size(); ++i) {
            if (alternatives[i].name == "UseShieldBlock"
                || (alternatives[i].name == "ReactiveStrike" && !has_allies)) {
                return i;
            }
        }
        return 0;
    }
    if (kind == "source") {
        size_t selected = 0;
        int largest_die = -1;
        for (size_t i = 0; i < alternatives.Size(); ++i) {
            const auto* source = std::any_cast<TGeneratedSourceCandidate<TWeapon>>(&alternatives[i].value);
            if (source != nullptr && source->Read().BaseDiceSize() > largest_die) {
                largest_die = source->Read().BaseDiceSize();
                selected = i;
            }
        }
        return selected;
    }
    if (kind == "move to position" && actor != nullptr) {
        size_t selected = 0;
        int best = std::numeric_limits<int>::max();
        for (size_t i = 0; i < alternatives.Size(); ++i) {
            const int distance = EnemyDistance(battle_, actor, alternatives[i].Get<TPosition>());
            if (distance < best) {
                best = distance;
                selected = i;
            }
        }
        return selected;
    }
    if (kind == "choice" && actor != nullptr) {
        const bool reachable = EnemyDistance(battle_, actor, actor->GetPosition())
            <= actor->GetCreature()->MaxWeaponReach();
        for (size_t i = 0; i < alternatives.Size(); ++i) {
            const auto* choice = std::any_cast<EMovementChoice>(&alternatives[i].value);
            if (choice != nullptr && *choice == (reachable ? EMovementChoice::FinishMovement
                                                           : EMovementChoice::ContinueMovement)) {
                return i;
            }
        }
    }
    return 0;
}

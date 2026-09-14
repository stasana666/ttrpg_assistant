#include <pf2e_engine/actions/generated_world.h>

#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/rule_integer.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/player.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>

TGeneratedWorldView::TGeneratedWorldView(const TBattle* battle)
    : battle_(battle)
{
}

bool TGeneratedWorldView::IsBound() const
{
    return battle_ != nullptr;
}

void TGeneratedWorldView::RequireWorld() const
{
    if (!IsBound()) {
        throw std::invalid_argument("world query requires a bound battle");
    }
}

void TGeneratedWorldView::Validate(const TGeneratedEntityView& entity) const
{
    RequireWorld();
    const auto* participant = &entity.Player();
    if (battle_->GetPlayer([participant](const TPlayer* current) {
            return current == participant;
        }) == nullptr) {
        throw TAbilityUnavailable();
    }
}

TPosition TGeneratedWorldView::Position(const TGeneratedEntityView& entity) const
{
    Validate(entity);
    return entity.Player().GetPosition();
}

int TGeneratedWorldView::PositionX(const TGeneratedEntityView& entity) const
{
    return Position(entity).x;
}

int TGeneratedWorldView::PositionY(const TGeneratedEntityView& entity) const
{
    return Position(entity).y;
}

int TGeneratedWorldView::Team(const TGeneratedEntityView& entity) const
{
    Validate(entity);
    return entity.Player().GetTeam();
}

int TGeneratedWorldView::GridDistance(const TGeneratedEntityView& first,
                                      const TGeneratedEntityView& second) const
{
    const auto first_position = Position(first);
    const auto second_position = Position(second);
    const auto difference_x = static_cast<std::int64_t>(first_position.x) - second_position.x;
    const auto difference_y = static_cast<std::int64_t>(first_position.y) - second_position.y;
    return TRuleInteger::Checked(std::max(difference_x < 0 ? -difference_x : difference_x,
                                          difference_y < 0 ? -difference_y : difference_y));
}

bool TGeneratedWorldView::SameTeam(const TGeneratedEntityView& first,
                                   const TGeneratedEntityView& second) const
{
    return Team(first) == Team(second);
}

bool TGeneratedWorldView::HasLineOfEffect(const TGeneratedEntityView& first,
                                          const TGeneratedEntityView& second) const
{
    const auto first_position = Position(first);
    const auto second_position = Position(second);
    return battle_->BattleMap()->HasLine(first_position, second_position);
}

std::vector<TGeneratedEntityView> TGeneratedWorldView::EntityViews(std::type_index type) const
{
    RequireWorld();
    std::vector<TGeneratedEntityView> result;
    if (type != typeid(TCreature)) {
        return result;
    }
    for (const auto* player : battle_->GetIfPlayers([](const TPlayer*) { return true; })) {
        result.push_back(BindGeneratedActionEntity(*player, battle_));
    }
    return result;
}

TGeneratedWorldView GeneratedActionWorld(const std::shared_ptr<TActionContext>& context)
{
    ValidateActionActor(context);
    return TGeneratedWorldView(context->battle);
}

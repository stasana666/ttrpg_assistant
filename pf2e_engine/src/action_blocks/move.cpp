#include <move.h>

#include <pf2e_engine/player.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/alternatives.h>
#include <pf2e_engine/interaction_system.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/actions/reaction.h>
#include <pf2e_engine/actions/save_point.h>

static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("creature");
static const TGameObjectId kMovementId = TGameObjectIdManager::Instance().Register("movement");

constexpr int kDx[4] = {0, 1, 0, -1};
constexpr int kDy[4] = {1, 0, -1, 0};

void FMove::operator() (std::shared_ptr<TActionContext> ctx) const
{
    TPlayer& target = *std::get<TPlayer*>(input_.Get(kTargetId, ctx));
    int movement = std::get<int>(input_.Get(kMovementId, ctx));

    return TryMove(target, movement, ctx);
}

void FMove::TryMove(TPlayer& target, int movement, std::shared_ptr<TActionContext> ctx) const
{
    if (movement == 0) {
        return;
    }

    auto snapshot = ctx->battle->BattleMap();
    TAlternatives alternatives = TAlternatives::Create<TPosition>("move to position");
    alternatives.AddAlternative("Завершить перемещение", target.GetPosition());
    for (size_t dir = 0; dir < 4; ++dir) {
        TPosition position = target.GetPosition();
        position.x += kDx[dir];
        position.y += kDy[dir];

        if (position.x < 0 || position.x >= snapshot->GetXSize()) {
            continue;
        }
        if (position.y < 0 || position.y >= snapshot->GetYSize()) {
            continue;
        }
        if (snapshot->GetCell(position.x, position.y).player != nullptr
            && snapshot->GetCell(position.x, position.y).player != &target)
        {
            continue;
        }

        std::stringstream ss;
        ss << position.x << " " << position.y;

        alternatives.AddAlternative(ss.str(), position);
    }
    auto choice = ctx->io_system->ChooseAlternative<TPosition>(target.GetId(), alternatives);
    if (choice == target.GetPosition()) {
        return;
    }

    TSavepointCallback continue_movement = [this, target_ptr = &target, movement, ctx, choice]() {
        target_ptr->SetPosition(choice);
        this->TryMove(*target_ptr, movement - 1, ctx);
    };

    TTriggerContext trigger_context{
        .type = ETrigger::OnMove,
        .triggered_player = &target,
    };
    throw TSavepointStackUnwind(ctx->transformator->CurrentState(), std::move(continue_movement));
    throw TReactionStackUnwind(ctx->transformator->CurrentState(), std::move(continue_movement), trigger_context);
}

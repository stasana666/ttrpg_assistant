#include <move.h>

#include <pf2e_engine/player.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/alternatives.h>
#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/continuation.h>
#include <pf2e_engine/actions/reaction.h>

#include <memory>
#include <sstream>

static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("creature");
static const TGameObjectId kMovementId = TGameObjectIdManager::Instance().Register("movement");

constexpr int kDx[4] = {0, 1, 0, -1};
constexpr int kDy[4] = {1, 0, -1, 0};

void FMove::operator() (std::shared_ptr<TActionContext> ctx) const
{
    TPlayer* target = std::get<TPlayer*>(input_.Get(kTargetId, ctx));
    int movement = std::get<int>(input_.Get(kMovementId, ctx));

    // `budget` lives on the heap so it survives a suspension and is shared by the
    // condition and body across continuation copies. Step decrements it, or
    // zeroes it when the player chooses to stop early.
    auto budget = std::make_shared<int>(movement);
    continuation::While(
        [budget]() { return *budget > 0; },
        [this, ctx, target, budget]() { Step(*target, *budget, ctx); });
}

void FMove::Step(TPlayer& target, int& budget, std::shared_ptr<TActionContext> ctx) const
{
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
        budget = 0;
        return;
    }

    target.SetPosition(choice);
    --budget;

    // A move just happened: report the reaction opportunity. The interaction
    // system decides whether to resolve it now or to suspend us via a savepoint
    // (e.g. for a voice assistant); the engine just continues the loop afterwards.
    TTriggerContext trigger{
        .type = ETrigger::OnMove,
        .triggered_player = &target,
    };
    ctx->io_system->HandleReactionTrigger(trigger, ctx->transformator->CurrentState());
}

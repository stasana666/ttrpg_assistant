#include <move.h>

#include <pf2e_engine/player.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/alternatives.h>
#include <pf2e_engine/interaction_system.h>
#include <pf2e_engine/battle.h>

static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("creature");
static const TGameObjectId kMovementId = TGameObjectIdManager::Instance().Register("movement");

constexpr int kDx[4] = {0, 1, 0, -1};
constexpr int kDy[4] = {1, 0, -1, 0};

void FMove::operator() (TActionContext& ctx) const
{
    TPlayer& target = *std::get<TPlayer*>(input_.Get(kTargetId, ctx));
    int movement = std::get<int>(input_.Get(kMovementId, ctx));

    while (movement > 0) {
        auto snapshot = ctx.battle->BattleMap();
        TAlternatives<TPosition> alternatives("move to position");
        alternatives.AddAlternative(TAlternative<TPosition>{
            .name = "завершить перемещение",
            .value = target.GetPosition()
        });
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

            alternatives.AddAlternative(TAlternative<TPosition>{
                .name = ss.str(),
                .value = position
            });
        }
        auto choice = ctx.io_system->ChooseAlternative(target.GetId(), alternatives);
        if (choice == target.GetPosition()) {
            break;
        }
        target.SetPosition(choice);
        --movement;
    }
}

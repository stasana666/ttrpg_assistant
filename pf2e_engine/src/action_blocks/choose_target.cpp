#include <choose_target.h>

#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/common/visit.h>
#include "action_context.h"
#include "weapon.h"

#include <variant>

static const TGameObjectId kCenterId = TGameObjectIdManager::Instance().Register("center");
static const TGameObjectId kRadiusId = TGameObjectIdManager::Instance().Register("radius");
static const TGameObjectId kAreaId = TGameObjectIdManager::Instance().Register("type");

enum class EAreaType {
    Emanation,
};

EAreaType AreaTypeFromString(const std::string& s) {
    if (s == "emanation") {
        return EAreaType::Emanation;
    }
    abort();
}

void FChooseTarget::operator ()(TActionContext& ctx) const
{
    EAreaType type = AreaTypeFromString(input_.GetString(kAreaId));
    switch (type) {
        case EAreaType::Emanation: {
            EmanationHandle(ctx);
            break;
        }
    }
    abort();
}

void FChooseTarget::EmanationHandle(TActionContext& ctx) const
{
    TPosition center;
    std::visit(VisitorHelper{
        [&](TCreature* creature) {
            center = ctx.battle_map->GetPosition(creature);
        },
        [](auto&&) {
            throw std::logic_error("unexpected type");
        }
    }, input_.Get(kCenterId, ctx));

    size_t radius;
    std::visit(VisitorHelper{
        [&](TWeapon*) {
            radius = 1;
        },
        [](auto&&) {
            throw std::logic_error("unexpected type");
        }
    }, input_.Get(kRadiusId, ctx));

    auto targets = ctx.battle_map->GetIfPlayers([&](const TPlayer* player) {
        return ctx.battle_map->HasLine(center, player->position, radius);
    });

    ChooseTarget(targets, ctx);
}

void FChooseTarget::ChooseTarget(std::vector<TPlayer*> players, TActionContext& ctx) const
{
    // TODO: добавить логику
    if (players.empty()) {
        throw std::logic_error("no targets");
    }
    ctx.game_object_register.Add(output_, players[0]->creature);
}

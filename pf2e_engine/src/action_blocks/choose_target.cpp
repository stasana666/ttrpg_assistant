#include <choose_target.h>

#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/common/visit.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/battle.h>

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
            return;
        }
    }
    abort();
}

void FChooseTarget::EmanationHandle(TActionContext& ctx) const
{
    TPosition center;
    std::visit(VisitorHelper{
        [&](TPlayer* player) {
            center = player->position;
        },
        [](auto&&) {
            throw std::logic_error("unexpected type for center: FChooseTarget::EmanationHandle");
        }
    }, input_.Get(kCenterId, ctx));

    size_t radius;
    std::visit(VisitorHelper{
        [&](TWeapon*) {
            radius = 1;
        },
        [&](int r) {
            radius = r;
        },
        [](auto&&) {
            throw std::logic_error("unexpected type for radius: FChooseTarget::EmanationHandle");
        }
    }, input_.Get(kRadiusId, ctx));

    auto targets = ctx.battle->GetIfPlayers([&](const TPlayer* player) {
        return ctx.battle->BattleMap().HasLine(center, player->position, radius);
    });

    ChooseTarget(targets, ctx);
}

void FChooseTarget::ChooseTarget(std::vector<TPlayer*> players, TActionContext& ctx) const
{
    // TODO: добавить логику
    TPlayer* self = &ctx.game_object_registry->Get<TPlayer>(TGameObjectIdManager::Instance().Register("self"));
    if (players.empty()) {
        throw std::logic_error("no targets");
    }
    for (auto& p : players) {
        if (p == self) {
            continue;
        }
        ctx.game_object_registry->Add(output_, p);
    }
}

#include <choose_target.h>

#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/common/visit.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/alternatives.h>
#include <pf2e_engine/interaction_system.h>

#include <sstream>
#include <variant>

static const TGameObjectId kSelfId = TGameObjectIdManager::Instance().Register("self");
static const TGameObjectId kCenterId = TGameObjectIdManager::Instance().Register("center");
static const TGameObjectId kRadiusId = TGameObjectIdManager::Instance().Register("radius");
static const TGameObjectId kAreaId = TGameObjectIdManager::Instance().Register("type");
static const TGameObjectId kRangeId = TGameObjectIdManager::Instance().Register("range");
static const TGameObjectId kApexId = TGameObjectIdManager::Instance().Register("apex");
static const TGameObjectId kStartId = TGameObjectIdManager::Instance().Register("start");
static const TGameObjectId kLengthId = TGameObjectIdManager::Instance().Register("length");
static const TGameObjectId kWidthId = TGameObjectIdManager::Instance().Register("width");

enum class EAreaType {
    Emanation,
    Burst,
    Cone,
    Line,
};

EAreaType AreaTypeFromString(const std::string& s) {
    if (s == "emanation") {
        return EAreaType::Emanation;
    }
    if (s == "burst") {
        return EAreaType::Burst;
    }
    if (s == "cone") {
        return EAreaType::Cone;
    }
    if (s == "line") {
        return EAreaType::Line;
    }
    abort();
}

void FChooseTarget::operator ()(std::shared_ptr<TActionContext> ctx) const
{
    EAreaType type = AreaTypeFromString(input_.GetString(kAreaId));
    switch (type) {
        case EAreaType::Emanation: {
            EmanationHandle(ctx);
            return;
        }
        case EAreaType::Burst: {
            BurstHandle(ctx);
            return;
        }
        case EAreaType::Cone: {
            ConeHandle(ctx);
            return;
        }
        case EAreaType::Line: {
            LineHandle(ctx);
            return;
        }
    }
    abort();
}

void FChooseTarget::EmanationHandle(std::shared_ptr<TActionContext> ctx) const
{
    TPosition center;
    std::visit(VisitorHelper{
        [&](TPlayer* player) {
            center = player->GetPosition();
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

    auto targets = ctx->battle->GetIfPlayers([&](const TPlayer* player) {
        return ctx->battle->BattleMap()->HasLine(center, player->GetPosition(), radius);
    });

    ChooseTarget(targets, ctx);
}

void FChooseTarget::BurstHandle(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer* self = &ctx->game_object_registry->Get<TPlayer>(kSelfId);
    TPosition caster_pos = self->GetPosition();
    int range = input_.GetNumber(kRangeId);
    int radius = input_.GetNumber(kRadiusId);

    auto battle_map = ctx->battle->BattleMap();

    TAlternatives<TPosition> alternatives("burst center");
    for (int x = 0; x < battle_map->GetXSize(); ++x) {
        for (int y = 0; y < battle_map->GetYSize(); ++y) {
            TPosition pos{x, y};
            if (battle_map->InRadius(caster_pos, range, pos)) {
                std::stringstream ss;
                ss << x << " " << y;
                alternatives.AddAlternative(TAlternative<TPosition>{
                    .name = ss.str(),
                    .value = pos
                });
            }
        }
    }

    TPosition burst_center = ctx->io_system->ChooseAlternative(self->GetId(), alternatives);

    auto targets = ctx->battle->GetIfPlayers([&](const TPlayer* player) {
        return battle_map->InRadius(burst_center, radius, player->GetPosition());
    });

    ChooseTarget(targets, ctx);
}

void FChooseTarget::ConeHandle(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer* self = &ctx->game_object_registry->Get<TPlayer>(kSelfId);
    TPosition apex = self->GetPosition();
    int length = input_.GetNumber(kLengthId);

    auto battle_map = ctx->battle->BattleMap();

    TAlternatives<TPosition> alternatives("cone direction");
    for (int x = 0; x < battle_map->GetXSize(); ++x) {
        for (int y = 0; y < battle_map->GetYSize(); ++y) {
            TPosition pos{x, y};
            if (pos == apex) {
                continue;
            }
            std::stringstream ss;
            ss << x << " " << y;
            alternatives.AddAlternative(TAlternative<TPosition>{
                .name = ss.str(),
                .value = pos
            });
        }
    }

    TPosition direction_cell = ctx->io_system->ChooseAlternative(self->GetId(), alternatives);

    auto targets = ctx->battle->GetIfPlayers([&](const TPlayer* player) {
        return battle_map->InCone(apex, direction_cell, length, player->GetPosition());
    });

    ChooseTarget(targets, ctx);
}

void FChooseTarget::LineHandle(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer* self = &ctx->game_object_registry->Get<TPlayer>(kSelfId);
    TPosition start = self->GetPosition();
    int length = input_.GetNumber(kLengthId);
    int width = input_.GetNumber(kWidthId);

    auto battle_map = ctx->battle->BattleMap();

    TAlternatives<TPosition> alternatives("line direction");
    for (int x = 0; x < battle_map->GetXSize(); ++x) {
        for (int y = 0; y < battle_map->GetYSize(); ++y) {
            TPosition pos{x, y};
            if (pos == start) {
                continue;
            }
            std::stringstream ss;
            ss << x << " " << y;
            alternatives.AddAlternative(TAlternative<TPosition>{
                .name = ss.str(),
                .value = pos
            });
        }
    }

    TPosition direction_cell = ctx->io_system->ChooseAlternative(self->GetId(), alternatives);

    auto targets = ctx->battle->GetIfPlayers([&](const TPlayer* player) {
        return battle_map->InLine(start, direction_cell, length, width, player->GetPosition());
    });

    ChooseTarget(targets, ctx);
}

void FChooseTarget::ChooseTarget(std::vector<TPlayer*> players, std::shared_ptr<TActionContext> ctx) const
{
    // TODO: добавить логику
    TPlayer* self = &ctx->game_object_registry->Get<TPlayer>(kSelfId);
    if (players.empty()) {
        throw std::logic_error("no targets");
    }
    for (auto& p : players) {
        if (p == self) {
            continue;
        }
        ctx->game_object_registry->Add(output_, p);
    }
}

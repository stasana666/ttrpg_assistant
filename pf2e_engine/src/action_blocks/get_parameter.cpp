#include <get_parameter.h>

#include <pf2e_engine/player.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

#include <stdexcept>

static const TGameObjectId kTargetId = TGameObjectIdManager::Instance().Register("creature");
static const TGameObjectId kTypeId = TGameObjectIdManager::Instance().Register("type");

void FGetParameter::operator() (std::shared_ptr<TActionContext> ctx) const
{
    std::string str_type = input_.GetString(kTypeId);

    if (str_type == "movement") {
        return MovementHandle(ctx);
    }

    throw std::invalid_argument("unknown parameter type '" + str_type + "'");
}

void FGetParameter::MovementHandle(std::shared_ptr<TActionContext> ctx) const
{
    TPlayer& target = *std::get<TPlayer*>(input_.Get(kTargetId, ctx));
    ctx->game_object_registry->Add(output_, target.GetCreature()->Movement());
}

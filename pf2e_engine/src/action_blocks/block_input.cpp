#include <block_input.h>

#include <pf2e_engine/common/visit.h>
#include "game_object_id.h"

void TBlockInput::Add(TGameObjectId key, InputValue value)
{
    input_mapping_.insert({key, std::move(value)});
}

std::string TBlockInput::GetString(TGameObjectId key) const
{
    return std::get<std::string>(input_mapping_.at(key));
}

TGameObjectPtr TBlockInput::Get(TGameObjectId key, TActionContext& ctx) const
{
    auto value = input_mapping_.at(key);
    TGameObjectPtr result;
    std::visit(VisitorHelper{
        [&, this](TGameObjectId id) {
            result = ctx.game_object_register.GetGameObjectPtr(id);
        },
        [&, this](const std::string& s) {
            result = std::make_unique<std::string>(s);
        }
    }, value);
    return result;
}

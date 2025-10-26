#include <block_input.h>

#include <pf2e_engine/common/visit.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <iostream>

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
    if (!input_mapping_.contains(key)) {
        std::cerr << TGameObjectIdManager::Instance().Name(key) << " - not found in block input" << std::endl;
    }
    auto value = input_mapping_.at(key);
    TGameObjectPtr result;
    std::visit(VisitorHelper{
        [&](TGameObjectId id) {
            result = ctx.game_object_registry->GetGameObjectPtr(id);
        },
        [&](const std::string& s) {
            result = s;
        }
    }, value);
    return result;
}

#pragma once

#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/actions/action_context.h>

#include <unordered_map>
#include <variant>

using InputValue = std::variant<
    std::string,
    TGameObjectId
>;

class TBlockInput {
public:
    void Add(TGameObjectId key, InputValue value);

    std::string GetString(TGameObjectId key) const;
    TGameObjectPtr Get(TGameObjectId key, TActionContext& ctx) const;

private:
    std::unordered_map<TGameObjectId, InputValue, TGameObjectIdHash> input_mapping_;
};

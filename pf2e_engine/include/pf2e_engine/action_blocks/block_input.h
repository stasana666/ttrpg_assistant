#pragma once

#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/expressions/base_expression.h>

#include <memory>
#include <unordered_map>
#include <variant>

using TDamageTable = std::unordered_map<std::string, std::shared_ptr<IExpression>>;

using InputValue = std::variant<
    std::string,
    int,
    TGameObjectId,
    TDamageTable
>;

class TBlockInput {
public:
    void Add(TGameObjectId key, InputValue value);

    std::string GetString(TGameObjectId key) const;
    int GetNumber(TGameObjectId key) const;
    const TDamageTable& GetDamageTable(TGameObjectId key) const;
    TGameObjectPtr Get(TGameObjectId key, std::shared_ptr<TActionContext> ctx) const;

private:
    std::unordered_map<TGameObjectId, InputValue, TGameObjectIdHash> input_mapping_;
};

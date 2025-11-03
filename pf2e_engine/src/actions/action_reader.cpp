#include <action_reader.h>

#include <action_block.h>

#include <pf2e_engine/action_blocks/add_condition.h>
#include <pf2e_engine/action_blocks/block_input.h>
#include <pf2e_engine/action_blocks/calculate_difficulty_class.h>
#include <pf2e_engine/action_blocks/choose_target.h>
#include <pf2e_engine/action_blocks/choose_weapon.h>
#include <pf2e_engine/action_blocks/deal_damage.h>
#include <pf2e_engine/action_blocks/roll_against_difficulty_class.h>
#include <pf2e_engine/action_blocks/weapon_damage_roll.h>

#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/common/errors.h>

#include <pf2e_engine/success_level.h>
#include <pf2e_engine/resources.h>

#include <nlohmann/json.hpp>

#include <memory>
#include <stdexcept>
#include <unordered_map>

const std::unordered_map<EBlockType, std::function<IActionBlock*()>> kEmptyBlockFactory{
    { EBlockType::FunctionCall, []() -> IActionBlock* { return new TFunctionCallBlock{}; }},
    { EBlockType::Switch, []() -> IActionBlock* { return new TSwitchBlock{}; }},
    { EBlockType::Terminate, []() -> IActionBlock* { return new TTerminateBlock{}; }},
};

const std::unordered_map<EBlockType, TActionReader::FBlockFiller>
TActionReader::kBlockFillerMapping{
    { EBlockType::FunctionCall, &TActionReader::FillFunctionCall },
    { EBlockType::Switch, &TActionReader::FillSwitch },
    { EBlockType::Terminate, &TActionReader::FillTerminate },
};

const std::unordered_map<std::string, TActionReader::FBlockFunction>
TActionReader::kFunctionMapping{
    { "add_condition", [](TBlockInput&& input, TGameObjectId output) { return FAddCondition(std::move(input), output); } },
    { "calculate_DC", [](TBlockInput&& input, TGameObjectId output) { return FCalculateDifficultyClass(std::move(input), output); } },
    { "choose_target", [](TBlockInput&& input, TGameObjectId output) { return FChooseTarget(std::move(input), output); } },
    { "choose_weapon", [](TBlockInput&& input, TGameObjectId output) { return FChooseWeapon(std::move(input), output); } },
    { "crit_weapon_damage_roll", [](TBlockInput&& input, TGameObjectId output) { return FCritWeaponDamageRoll(std::move(input), output); } },
    { "deal_damage", [](TBlockInput&& input, TGameObjectId output) { return FDealDamage(std::move(input), output); } },
    { "roll_against_DC", [](TBlockInput&& input, TGameObjectId output) { return FRollAgainstDifficultyClass(std::move(input), output); } },
    { "weapon_damage_roll", [](TBlockInput&& input, TGameObjectId output) { return FWeaponDamageRoll(std::move(input), output); } },
};

TAction TActionReader::ReadAction(nlohmann::json& json)
{
    auto pipeline = ReadBlocks(json["pipeline"]);
    TAction::TResources resources = ReadResources(json["resources"]);
    TAction action(std::move(pipeline), std::move(resources), json["name"]);

    Clear();
    return action;
}

TAction::TResources TActionReader::ReadResources(nlohmann::json& json) const
{
    TAction::TResources resources;
    for (auto& resource : json) {
        auto resource_id = TResourceIdManager::Instance().Register(resource["name"]);
        resources.push_back(TAction::TResource{
            .resource_id = resource_id,
            .count = resource["count"]
        });
    }
    return resources;
}

auto TActionReader::ReadBlocks(nlohmann::json& json) ->TAction::TPipeline
{
    pipeline_.reserve(json.size());
    for (auto block : json) {
        EBlockType type = BlockTypeFromString(block["type"]);
        pipeline_.emplace_back(kEmptyBlockFactory.at(type)());

        auto id = id_register_.Register(block["name"]);
        pipeline_.back()->name_ = block["name"];

        pipeline_place_.insert({pipeline_.back().get(), std::prev(pipeline_.end())});
        block_mapping_[id] = pipeline_.back().get();
    }
    for (auto& block : json) {
        EBlockType type = BlockTypeFromString(block["type"]);
        auto id = id_register_.Register(block["name"]);
        (this->*(kBlockFillerMapping.at(type)))(block, block_mapping_.at(id));
    }

    return std::move(pipeline_);
}

void TActionReader::FillFunctionCall(nlohmann::json& json, IActionBlock* block)
{
    TFunctionCallBlock* function_block = dynamic_cast<TFunctionCallBlock*>(block);

    FunctionCallFillFunction(json, function_block);
    FunctionCallFillNext(json, function_block);
}

void TActionReader::FunctionCallFillNext(nlohmann::json& json, TFunctionCallBlock* function_block)
{
    if (json.contains("next")) {
        auto next_id = id_register_.Register(json["next"]);
        function_block->next_ = block_mapping_.at(next_id);
    } else {
        auto it = pipeline_place_.at(function_block);
        ++it;
        assert(it != pipeline_.end());
        function_block->next_ = it->get();
    }
}

void TActionReader::FunctionCallFillFunction(nlohmann::json& json, TFunctionCallBlock* function_block)
{
    std::string function_name = json["function"];

    nlohmann::json& input = json["input"];
    nlohmann::json& output = json["output"];

    TGameObjectId output_id = TGameObjectIdManager::Instance().Register(output[0]);

    auto constructor = kFunctionMapping.find(function_name);
    if (constructor == kFunctionMapping.end()) {
        throw std::invalid_argument("unknown function block: " + function_name);
    }
    function_block->apply_ = constructor->second(ReadInput(input), output_id);
}

TBlockInput TActionReader::ReadInput(nlohmann::json& json) const
{
    TBlockInput input;
    for (auto& [key, value] : json.items()) {
        TGameObjectId key_id =  TGameObjectIdManager::Instance().Register(key);
        if (value.is_string()) {
            std::string str_value = value;
            if (str_value[0] == '$') {
                input.Add(key_id, TGameObjectIdManager::Instance().Register(str_value.substr(1)));
            } else {
                input.Add(key_id, str_value);
            }
        } else if (value.is_number()) {
            input.Add(key_id, value.get<int>());
        }
    }
    return input;
}

void TActionReader::FillSwitch(nlohmann::json& json, IActionBlock* block)
{
    TSwitchBlock* switch_block = dynamic_cast<TSwitchBlock*>(block);

    auto add_next = [&](ESuccessLevel success_level) {
        auto id = id_register_.Register(json["next_table"][ToString(success_level)]);
        switch_block->next_table_[success_level] = block_mapping_.at(id);
    };

    add_next(ESuccessLevel::CriticalFailure);
    add_next(ESuccessLevel::Failure);
    add_next(ESuccessLevel::Success);
    add_next(ESuccessLevel::CriticalSuccess);

    switch_block->input_ = ReadInput(json["input"]);
}

void TActionReader::FillTerminate(nlohmann::json&, IActionBlock*)
{
}

void TActionReader::Clear()
{
    block_mapping_.clear();
    pipeline_place_.clear();
    pipeline_.clear();
}

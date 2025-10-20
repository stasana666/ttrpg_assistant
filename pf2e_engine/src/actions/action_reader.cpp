#include <action_reader.h>
#include <memory>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "action_block.h"
#include "action_context.h"
#include "attack_roll.h"
#include "block_input.h"
#include "choose_target.h"
#include "choose_weapon.h"
#include "game_object_id.h"
#include "success_level.h"

#include <unordered_map>

const std::unordered_map<EBlockType, std::function<IActionBlock*()>> kEmptyBlockFactory{
    { EBlockType::FunctionCall, []() -> IActionBlock* { return new TFunctionCallBlock{}; }},
    { EBlockType::Switch, []() -> IActionBlock* { return new TSwitchBlock{}; }},
};

const std::unordered_map<EBlockType, TActionReader::FBlockFiller>
TActionReader::kBlockFillerMapping{
    { EBlockType::FunctionCall, &TActionReader::FillFunctionCall },
    { EBlockType::Switch, &TActionReader::FillSwitch },
    { EBlockType::Terminate, &TActionReader::FillTerminate },
};

const std::unordered_map<std::string, TActionReader::FBlockFunction>
TActionReader::kFunctionMapping{
    { "choose_weapon", [](TBlockInput&& input, TGameObjectId output) { return FChooseWeapon(std::move(input), output); } },
    { "choose_target", [](TBlockInput&& input, TGameObjectId output) { return FChooseTarget(std::move(input), output); } },
    { "attack_roll", [](TBlockInput&& input, TGameObjectId output) { return FAttackRoll(std::move(input), output); } },
};

bool TActionReader::ValidateAction(nlohmann::json&)
{
    return true;
}

TAction TActionReader::ReadAction(nlohmann::json& json)
{
    if (!ValidateAction(json)) {
        throw std::runtime_error("wrong action format");
    }

    auto pipeline = ReadBlocks(json["pf2e_action"]["pipeline"]);

    return {};
}

auto TActionReader::ReadBlocks(nlohmann::json& json) -> TPipeline
{
    for (auto block : json) {
        EBlockType type = BlockTypeFromString(block["type"]);
        pipeline_.emplace_back(kEmptyBlockFactory.at(type)());
        auto id = id_register_.Register(block["name"]);
        block_mapping_[id] = pipeline_.back().get();
        pipeline_.back()->name_ = block["name"];
    }

    for (auto& block : json) {
        EBlockType type = BlockTypeFromString(block["type"]);
        auto id = id_register_.Register(block["name"]);
        (this->*(kBlockFillerMapping.at(type)))(block, block_mapping_[id]);
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
        throw std::logic_error("unknown function block");
    }
    function_block->apply_ = constructor->second(ReadInput(input), output_id);
}

TBlockInput TActionReader::ReadInput(nlohmann::json& json) const
{
    TBlockInput input;
    for (auto& [key, value] : json.items()) {
        TGameObjectId key_id =  TGameObjectIdManager::Instance().Register(key);
        std::string str_value = value;
        if (str_value[0] == '$') {
            input.Add(key_id, TGameObjectIdManager::Instance().Register(str_value.substr(1)));
        } else {
            input.Add(key_id, str_value);
        }
    }
    return input;
}

void TActionReader::FillSwitch(nlohmann::json& json, IActionBlock* block)
{
    TSwitchBlock* switch_block = dynamic_cast<TSwitchBlock*>(block);

    auto add_next = [&](ESuccessLevel success_level) {
        auto id = id_register_.Register(json[ToString(success_level)]);
        switch_block->next_table_[success_level] = block_mapping_.at(id);
    };

    add_next(ESuccessLevel::CriticalFailure);
    add_next(ESuccessLevel::Failure);
    add_next(ESuccessLevel::Success);
    add_next(ESuccessLevel::CriticalSuccess);

    switch_block->input_ = ReadInput(json["input"]);
}

void TActionReader::Clear()
{
    block_mapping_.clear();
    pipeline_place_.clear();
    pipeline_.clear();
}

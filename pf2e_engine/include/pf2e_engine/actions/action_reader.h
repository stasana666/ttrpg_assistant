#pragma once

#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/actions/action_block.h>

#include <pf2e_engine/action_blocks/block_input.h>

#include <pf2e_engine/common/value_id.h>

#include <nlohmann/json_fwd.hpp>

using TActionId = TValueId<struct ActionTag>;
using TActionIdHasher = TValueIdHash<struct ActionTag>;
using TActionIdManager = TValueIdManager<struct ActionTag>;

class TActionReader {
public:
    TAction ReadAction(nlohmann::json&);

private:
    using FBlockFiller = void(TActionReader::*)(nlohmann::json&, IActionBlock*);
    using FBlockFunction = std::function<std::function<void(TActionContext&)>(TBlockInput&&, TGameObjectId)>;
    static const std::unordered_map<EBlockType, FBlockFiller> kBlockFillerMapping;
    static const std::unordered_map<std::string, FBlockFunction> kFunctionMapping;

    std::vector<std::unique_ptr<IActionBlock>> ReadBlocks(nlohmann::json& json);

    TBlockInput ReadInput(nlohmann::json& json) const;

    void FillFunctionCall(nlohmann::json&, IActionBlock*);
    void FillSwitch(nlohmann::json&, IActionBlock*);
    void FillTerminate(nlohmann::json&, IActionBlock*);

    void FunctionCallFillNext(nlohmann::json&, TFunctionCallBlock*);
    void FunctionCallFillFunction(nlohmann::json&, TFunctionCallBlock*);

    void Clear();

    std::unordered_map<TActionId, IActionBlock*, TActionIdHasher> block_mapping_;
    TAction::TPipeline pipeline_;
    std::unordered_map<IActionBlock*, TAction::TPipeline::iterator> pipeline_place_;
    TActionIdManager id_register_;
};

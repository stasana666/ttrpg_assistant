#include <action_block.h>

#include <pf2e_engine/common/errors.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/success_level.h>
#include <pf2e_engine/common/visit.h>

#include <cassert>
#include <stdexcept>
#include <sstream>
#include "save_point.h"

const TGameObjectId kSuccessLevelId = TGameObjectIdManager::Instance().Register("value");

constexpr std::string ToString(EBlockType type)
{
    switch (type) {
        case EBlockType::Switch:
            return "switch";
        case EBlockType::FunctionCall:
            return "function_call";
        case EBlockType::Terminate:
            return "terminate";
    }
    throw std::logic_error("unexpected BlockType");
}

EBlockType BlockTypeFromString(const std::string& type)
{
    for (auto t : {EBlockType::FunctionCall, EBlockType::Switch, EBlockType::Terminate}) {
        if (type == ToString(t)) {
            return t;
        }
    }
    std::stringstream ss;
    ss << "unknown block type " << type << "\n";
    throw std::runtime_error(ss.str());
}

void TFunctionCallBlock::Apply(std::shared_ptr<TActionContext> ctx)
{
    ApplyHelper([apply = &apply_, &ctx](){ (*apply)(ctx); }, ctx);
}

void TFunctionCallBlock::ApplyHelper(std::function<void()> apply, std::shared_ptr<TActionContext> ctx)
{
    try {
        apply();
        ctx->next_block = next_;
    } catch (TSavepointStackUnwind& save_point) {
        save_point.AddCallFunctionLevel([this, ctx](TSavepointCallback callback) {
            ApplyHelper(callback, ctx);
        });
        throw save_point;
    }
}

void TSwitchBlock::Apply(std::shared_ptr<TActionContext> ctx)
{
    std::visit(VisitorHelper{
        [&](ESuccessLevel success_level) {
            ctx->next_block = next_table_[success_level];
        },
        [](auto&&) {
            throw std::logic_error("unexpected type");
        }
    }, input_.Get(kSuccessLevelId, ctx));
}

void TTerminateBlock::Apply(std::shared_ptr<TActionContext> ctx)
{
    ctx->next_block = nullptr;
}

#include <pf2e_engine/common/errors.h>

#include <action_block.h>
#include <cassert>
#include <stdexcept>
#include <sstream>

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

void TFunctionCallBlock::Apply(TActionContext&)
{
    throw ToDoError("TFunctionCallBlock::Apply(TActionContext& ctx)");
}

void TSwitchBlock::Apply(TActionContext&)
{
    throw ToDoError("TSwitchBlock::Apply(TActionContext& ctx)");
}

void TTerminateBlock::Apply(TActionContext&)
{
    throw ToDoError("TTerminateBlock::Apply(TActionContext& ctx)");
}

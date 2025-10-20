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
    }
    assert(false);
}

EBlockType BlockTypeFromString(const std::string& type)
{
    for (auto t : {EBlockType::FunctionCall, EBlockType::Switch}) {
        if (type == ToString(t)) {
            return t;
        }
    }
    std::stringstream ss;
    ss << "unknown block type " << type << "\n";
    throw std::runtime_error(ss.str());
}

#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct TActionFrameSlot {
    std::string CppType;
    std::string Local;
};

struct TActionFlowChoice {
    std::string Type;
    std::string CppType;
    std::string Local;
    std::string Candidates;
    std::vector<std::string> Predicate{};
    std::size_t Slot = 0;
    bool Entity = false;
};

struct TActionFlowBlock {
    enum class EExit { Return, Jump, Branch, Choose };
    std::vector<std::size_t> LiveSlots;
    std::vector<std::string> Body;
    EExit Exit = EExit::Return;
    std::size_t Next = 0;
    std::size_t Otherwise = 0;
    std::string Condition;
    TActionFlowChoice Choice;
};

struct TCompiledActionFlow {
    std::vector<TActionFrameSlot> Slots;
    std::vector<TActionFlowBlock> Blocks;
    std::vector<std::string> Environment;
    std::size_t Entry = 0;
    std::string FlowType = "TFlow";
    std::string FrameType = "TFrame";
    std::string RunName = "Run";
};

std::vector<std::string> RenderActionFlow(const TCompiledActionFlow& flow);

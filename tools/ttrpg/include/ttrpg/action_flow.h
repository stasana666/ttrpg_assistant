#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
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
    bool Source = false;
    std::string PredicateCppType;
};

struct TActionFlowBlock {
    enum class EExit { Return, Jump, Branch, Choose, Invoke, Window, Publish };
    std::vector<std::size_t> LiveSlots;
    std::vector<std::string> Body;
    EExit Exit = EExit::Return;
    std::size_t Next = 0;
    std::size_t Otherwise = 0;
    std::string Condition;
    TActionFlowChoice Choice;
    struct TInvocation {
        std::string FunctionName;
        std::string ResultType;
        std::vector<std::string> Arguments;
        std::optional<std::size_t> Slot;
        std::vector<std::string> InputValidators;
    } Invocation;
    struct TWindow {
        std::string Tag;
        std::string StateType;
        std::vector<std::string> Arguments;
        std::optional<std::size_t> Slot;
    } Window;
    std::string ReturnValue;
    std::string Publication;
};

struct TCompiledActionFlow {
    std::vector<TActionFrameSlot> Slots;
    std::vector<TActionFlowBlock> Blocks;
    std::vector<std::string> Environment;
    std::size_t Entry = 0;
    std::string FlowType = "TFlow";
    std::string FrameType = "TFrame";
    std::string RunName = "Run";
    std::string OwnerType;
    std::string ResultType = "void";
    bool Activity = false;
    bool HasReactionScope = false;
    bool HasActionTicket = false;
    std::vector<std::string> EntryChecks;
    std::vector<std::pair<std::size_t, std::string>> Initializers;
};

std::vector<std::string> RenderActionFlow(const TCompiledActionFlow& flow);

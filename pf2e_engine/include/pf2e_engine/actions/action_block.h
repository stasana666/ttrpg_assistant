#pragma once

#include "action_context.h"
#include "block_input.h"
#include "success_level.h"

class IActionBlock {
public:
    virtual ~IActionBlock() = default;

    virtual void Apply(TActionContext& ctx);

protected:
    std::string name_;

    friend class TActionReader;
};

enum class EBlockType {
    FunctionCall,
    Switch,
    Terminate,
};

constexpr std::string ToString(EBlockType);
EBlockType BlockTypeFromString(const std::string&);

class TFunctionCallBlock final : public IActionBlock {
public:
    void Apply(TActionContext& ctx) override;

private:
    std::function<void(TActionContext&)> apply_;
    IActionBlock* next_;

    friend class TActionReader;
};

class TSwitchBlock final : public IActionBlock {
public:
    IActionBlock* Next(ESuccessLevel) const;

private:
    std::unordered_map<ESuccessLevel, IActionBlock*> next_table_;
    TBlockInput input_;

    friend class TActionReader;
};

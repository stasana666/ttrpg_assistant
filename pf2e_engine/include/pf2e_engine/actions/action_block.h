#pragma once

#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/action_blocks/block_input.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/success_level.h>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class IActionBlock {
public:
    virtual ~IActionBlock() = default;

    virtual void Apply(std::shared_ptr<TActionContext> ctx) = 0;

    virtual std::string GetName() const { return name_; }

    void Run(std::shared_ptr<TActionContext> ctx);

protected:
    std::string name_;

    friend class TPipelineReader;
};

enum class EBlockType {
    FunctionCall,
    Switch,
    Terminate,
    ForEach,
};

constexpr std::string ToString(EBlockType);
EBlockType BlockTypeFromString(const std::string&);

class TFunctionCallBlock final : public IActionBlock {
public:
    void Apply(std::shared_ptr<TActionContext> ctx) override;
    std::string GetName() const override { return function_name_; }

private:
    std::function<void(std::shared_ptr<TActionContext>)> apply_;
    std::string function_name_;
    IActionBlock* next_;

    friend class TPipelineReader;
};

class TSwitchBlock final : public IActionBlock {
public:
    void Apply(std::shared_ptr<TActionContext> ctx) override;

private:
    std::unordered_map<ESuccessLevel, IActionBlock*> next_table_;
    TBlockInput input_;

    friend class TPipelineReader;
};

class TTerminateBlock final : public IActionBlock {
public:
    void Apply(std::shared_ptr<TActionContext> ctx) override;

private:
    friend class TPipelineReader;
};

class TForEachBlock final : public IActionBlock {
public:
    void Apply(std::shared_ptr<TActionContext> ctx) override;

private:
    TBlockInput input_;
    std::optional<TGameObjectId> element_id_;
    std::vector<std::unique_ptr<IActionBlock>> body_;
    IActionBlock* next_ = nullptr;

    friend class TPipelineReader;
};

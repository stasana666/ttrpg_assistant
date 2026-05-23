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

    // Runs the block's primary work. Override to implement behaviour.
    virtual void Apply(std::shared_ptr<TActionContext> ctx) = 0;

    // Identifies *which* block this is for the purpose of feat hooks.
    // Default returns the per-pipeline JSON name; TFunctionCallBlock overrides
    // to return the registered function name (e.g. "weapon_damage_roll") so
    // feats can hook a function across every action that uses it.
    virtual std::string GetName() const { return name_; }

    // Non-virtual entry point: runs every creature feat whose `block` matches
    // GetName() (on $self), then calls Apply. All pipeline dispatch -- the
    // outer action loop, RunSubPipeline, ForEach bodies -- goes through Run
    // so feats fire wherever a block of the matching name runs.
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

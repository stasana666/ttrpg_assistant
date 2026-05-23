#pragma once

#include <pf2e_engine/actions/action_block.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/resources.h>

class TWeapon;

class TAction {
public:
    using TPipeline = std::vector<std::unique_ptr<IActionBlock>>;

    struct TResource {
        TResourceId resource_id;
        size_t count;
    };

    using TResources = std::vector<TResource>;

    // A game object defined declaratively in the action JSON and pre-loaded
    // into the action's registry before the pipeline runs.
    // TODO: only weapon variables for now; generalise to other object types.
    struct TActionVariable {
        TGameObjectId id;
        std::shared_ptr<TWeapon> weapon;
    };

    using TVariables = std::vector<TActionVariable>;

    TAction(TPipeline&& pipeline, TResources&& consume, std::string&& name,
            TVariables&& variables = {});

    void Apply(std::shared_ptr<TActionContext> ctx, TPlayer& player);
    void Consume(TPlayer& player);
    bool Check(const TPlayer& self);
    std::string_view Name() const;

private:
    TPipeline pipeline_;
    TResources consume_;
    std::string name_;
    TVariables variables_;
};

// Runs a block pipeline on an EXISTING context (reusing its registry), starting
// at `first`, then restores ctx->next_block. Used to run a creature feat's
// sub-pipeline during a parent action. The sub-pipeline must not suspend.
void RunSubPipeline(std::shared_ptr<TActionContext> ctx, IActionBlock* first);

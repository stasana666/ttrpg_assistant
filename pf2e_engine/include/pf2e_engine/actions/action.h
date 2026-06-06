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

    // TODO: only weapon variables for now; generalise to other object types.
    struct TActionVariable {
        TGameObjectId id;
        std::shared_ptr<TWeapon> weapon;
    };

    using TVariables = std::vector<TActionVariable>;

    TAction(TPipeline&& pipeline, TResources&& consume, std::string&& name,
            TVariables&& variables = {});

    void Apply(std::shared_ptr<TActionContext> ctx, TPlayer& player);
    void Consume(std::shared_ptr<TActionContext> ctx, TPlayer& player);
    bool Check(const TPlayer& self);
    std::string_view Name() const;

private:
    TPipeline pipeline_;
    TResources consume_;
    std::string name_;
    TVariables variables_;
};

void RunSubPipeline(std::shared_ptr<TActionContext> ctx, IActionBlock* first);

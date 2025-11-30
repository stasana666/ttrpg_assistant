#pragma once

#include <pf2e_engine/actions/action_block.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/resources.h>

class TAction {
public:
    using TPipeline = std::vector<std::unique_ptr<IActionBlock>>;

    struct TResource {
        TResourceId resource_id;
        size_t count;
    };

    using TResources = std::vector<TResource>;

    TAction(TPipeline&& pipeline, TResources&& consume, std::string&& name);

    void Apply(std::shared_ptr<TActionContext> ctx, TPlayer& player);
    void Consume(TPlayer& player);
    bool Check(const TPlayer& self);
    std::string_view Name() const;

private:
    void ApplyPipeline(std::function<void()> apply, std::shared_ptr<TActionContext> ctx);

    TPipeline pipeline_;
    TResources consume_;
    std::string name_;
};

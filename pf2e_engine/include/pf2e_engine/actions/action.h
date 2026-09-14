#pragma once

#include <pf2e_engine/actions/action_block.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/common/guarded.h>
#include <pf2e_engine/common/resource.h>

#include <functional>

class TWeapon;

enum class EResourceKind {
    Action,
    Reaction,
};

class TAction {
public:
    using TPipeline = std::vector<std::unique_ptr<IActionBlock>>;

    struct TResourceCost {
        EResourceKind kind;
        int count;
    };

    using TResources = std::vector<TResourceCost>;

    using FBody = std::function<void(std::shared_ptr<TActionContext>)>;

    struct TBoundResourceCost {
        std::string Key;
        int Count;
        std::function<const TResource&(const TPlayer&)> Read;
        std::function<TGuarded<TResource>(TPlayer&)> Write;
    };

    using TBoundResources = std::vector<TBoundResourceCost>;

    // TODO: only weapon variables for now; generalise to other object types.
    struct TActionVariable {
        TGameObjectId id;
        std::shared_ptr<const TWeapon> weapon;
    };

    using TVariables = std::vector<TActionVariable>;

    TAction(TPipeline&& pipeline, TResources&& consume, std::string&& name,
            TVariables&& variables = {});
    TAction(FBody body, TBoundResources costs, std::string name);

    void Apply(std::shared_ptr<TActionContext> ctx, TPlayer& player) const;
    void Consume(std::shared_ptr<TActionContext> ctx, TPlayer& player) const;
    bool Check(const TPlayer& self) const;
    std::string_view Name() const;

private:
    TPipeline pipeline_;
    FBody body_;
    TBoundResources consume_;
    std::string name_;
    TVariables variables_;
};

void RunSubPipeline(std::shared_ptr<TActionContext> ctx, IActionBlock* first);

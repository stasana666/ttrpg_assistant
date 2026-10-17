#pragma once

#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/resource_payment.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>

class TAction {
public:
    using FBody = std::function<void(std::shared_ptr<TActionContext>)>;

    using TBoundResourceCost = ::TBoundResourceCost;
    using TBoundResources = ::TBoundResources;

    struct TCompiledBody {
        FBody Run;
    };

    TAction(FBody body, TBoundResources costs, std::string name);
    TAction(TCompiledBody body, TBoundResources costs, std::string name);

    void Apply(std::shared_ptr<TActionContext> ctx, TPlayer& player) const;
    void Consume(std::shared_ptr<TActionContext> ctx, TPlayer& player) const;
    bool Check(const TPlayer& self) const;
    std::string_view Name() const;

private:
    FBody body_;
    TResourcePayment payment_;
    bool compiled_body_ = false;
    std::string name_;
};

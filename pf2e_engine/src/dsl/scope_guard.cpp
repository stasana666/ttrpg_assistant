#include <pf2e_engine/dsl/expression.h>

TScopeGuard::TScopeGuard(TEvalContext& ctx, std::string name, TDslValue value)
    : ctx_(ctx)
    , name_(std::move(name))
    , had_prior_(false)
{
    auto it = ctx_.scope.find(name_);
    if (it != ctx_.scope.end()) {
        saved_ = it->second;
        had_prior_ = true;
    }
    ctx_.scope.insert_or_assign(name_, std::move(value));
}

TScopeGuard::~TScopeGuard()
{
    if (had_prior_) {
        ctx_.scope.insert_or_assign(name_, std::move(*saved_));
    } else {
        ctx_.scope.erase(name_);
    }
}

void TScopeGuard::Rebind(TDslValue value)
{
    ctx_.scope.insert_or_assign(name_, std::move(value));
}

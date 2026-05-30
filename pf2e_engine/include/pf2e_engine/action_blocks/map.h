#pragma once

#include <pf2e_engine/action_blocks/base_function.h>
#include <pf2e_engine/dsl/expression.h>

#include <memory>

class FMap : public FBaseFunction {
public:
    FMap(TBlockInput&& input, TGameObjectId output);
    void operator()(std::shared_ptr<TActionContext> ctx) const;
private:
    std::shared_ptr<IDslExpression> expr_;
};

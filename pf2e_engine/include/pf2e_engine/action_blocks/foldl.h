#pragma once

#include <pf2e_engine/action_blocks/base_function.h>
#include <pf2e_engine/dsl/expression.h>

#include <memory>

class FFoldL : public FBaseFunction {
public:
    FFoldL(TBlockInput&& input, TGameObjectId output);
    void operator()(std::shared_ptr<TActionContext> ctx) const;
private:
    std::shared_ptr<IDslExpression> expr_;
    std::shared_ptr<IDslExpression> init_;  // null if seed comes from list head
};

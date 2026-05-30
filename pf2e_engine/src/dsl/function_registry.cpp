#include <pf2e_engine/dsl/function_registry.h>

#include <stdexcept>

TDslFunctionRegistry& TDslFunctionRegistry::Instance()
{
    static TDslFunctionRegistry inst;
    return inst;
}

void TDslFunctionRegistry::Register(std::string name, TFunction fn)
{
    fns_.insert({std::move(name), std::move(fn)});
}

TDslValue TDslFunctionRegistry::Call(const std::string& name, const std::vector<TDslValue>& args, TEvalContext& ctx) const
{
    auto it = fns_.find(name);
    if (it == fns_.end()) {
        throw std::runtime_error("dsl: unknown function '" + name + "'");
    }
    return it->second(args, ctx);
}

bool TDslFunctionRegistry::Has(const std::string& name) const
{
    return fns_.contains(name);
}

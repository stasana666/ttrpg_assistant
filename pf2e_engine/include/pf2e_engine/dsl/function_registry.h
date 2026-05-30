#pragma once

#include <pf2e_engine/dsl/expression.h>
#include <pf2e_engine/dsl/value.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class TDslFunctionRegistry {
public:
    using TFunction = std::function<TDslValue(const std::vector<TDslValue>&, TEvalContext&)>;

    static TDslFunctionRegistry& Instance();

    void Register(std::string name, TFunction fn);
    TDslValue Call(const std::string& name, const std::vector<TDslValue>& args, TEvalContext& ctx) const;
    bool Has(const std::string& name) const;

private:
    std::unordered_map<std::string, TFunction> fns_;
};

#pragma once

#include <pf2e_engine/common/guarded.h>
#include <pf2e_engine/common/resource.h>

#include <functional>
#include <string>
#include <vector>

class TPlayer;
class TTransformator;

struct TBoundResourceCost {
    std::string Key;
    int Count;
    std::function<const TResource&(const TPlayer&)> Read;
    std::function<TGuarded<TResource>(TPlayer&)> Write;
};

using TBoundResources = std::vector<TBoundResourceCost>;

class TResourcePayment {
public:
    explicit TResourcePayment(TBoundResources costs = {});
    bool Check(const TPlayer& actor) const;
    void Consume(TTransformator& journal, TPlayer& actor) const;

private:
    TBoundResources costs_;
};

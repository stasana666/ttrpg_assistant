#include <pf2e_engine/actions/resource_payment.h>

#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace {

void AddCost(int& total, int count)
{
    if (count < 0 || count > std::numeric_limits<int>::max() - total) {
        throw std::invalid_argument("invalid action resource cost");
    }
    total += count;
}

void ValidateCosts(const TBoundResources& costs)
{
    std::map<std::string, int, std::less<>> totals;
    for (const auto& cost : costs) {
        if (cost.Key.empty() || !cost.Read || !cost.Write) {
            throw std::invalid_argument("invalid action resource binding");
        }
        AddCost(totals[cost.Key], cost.Count);
    }
}

struct TResourceTotal {
    const TResource* Resource;
    std::size_t FirstBinding;
    int Count = 0;
};

struct TResolvedReadCosts {
    std::vector<const TResource*> Bindings;
    std::vector<TResourceTotal> Totals;
};

TResolvedReadCosts ResolveReadCosts(const TBoundResources& costs, const TPlayer& actor)
{
    TResolvedReadCosts resolved;
    std::map<std::string, const TResource*, std::less<>> keys;
    std::map<const TResource*, std::size_t> resources;
    for (const auto& cost : costs) {
        const auto* resource = &cost.Read(actor);
        const auto [key, key_inserted] = keys.emplace(cost.Key, resource);
        if (!key_inserted && key->second != resource) {
            throw std::invalid_argument("action resource key refers to different resources");
        }
        const auto [entry, inserted] = resources.emplace(resource, resolved.Totals.size());
        if (inserted) {
            resolved.Totals.push_back({resource, resolved.Bindings.size()});
        }
        AddCost(resolved.Totals[entry->second].Count, cost.Count);
        resolved.Bindings.push_back(resource);
    }
    return resolved;
}

}

TResourcePayment::TResourcePayment(TBoundResources costs)
    : costs_(std::move(costs))
{
    ValidateCosts(costs_);
}

void TResourcePayment::Consume(TTransformator& journal, TPlayer& player) const
{
    const auto resolved = ResolveReadCosts(costs_, std::as_const(player));
    std::vector<TGuarded<TResource>> targets;
    targets.reserve(costs_.size());
    for (std::size_t index = 0; index < costs_.size(); ++index) {
        auto target = costs_[index].Write(player);
        if (&target.Get() != resolved.Bindings[index]) {
            throw std::invalid_argument("action resource read and write bindings disagree");
        }
        targets.push_back(target);
    }
    for (const auto& total : resolved.Totals) {
        if (!total.Resource->Has(total.Count)) {
            throw TAbilityUnavailable();
        }
    }
    auto execution = journal.ExecutionContext();
    auto transaction = execution.Atomic();
    for (const auto& total : resolved.Totals) {
        if (total.Count != 0) {
            execution.SpendResource(targets[total.FirstBinding], total.Count,
                TGuarded<TCreature>(*player.GetCreature()), costs_[total.FirstBinding].Key);
        }
    }
    transaction.Commit();
}

bool TResourcePayment::Check(const TPlayer& player) const
{
    for (const auto& total : ResolveReadCosts(costs_, player).Totals) {
        if (!total.Resource->Has(total.Count)) {
            return false;
        }
    }
    return true;
}

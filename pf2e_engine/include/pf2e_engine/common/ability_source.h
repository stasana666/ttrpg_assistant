#pragma once

#include <pf2e_engine/common/ability.h>

#include <optional>
#include <map>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <class TSource>
struct TLocatedAbilitySource {
    const TSource* Value;
    TAbilitySourcePath Path;
};

template <class TSource>
class TAbilitySourceFinder {
public:
    explicit TAbilitySourceFinder(TAbilitySourcePath requested)
        : requested_(std::move(requested))
    {
    }

    bool ShouldVisit(const TAbilitySourcePath& path) const
    {
        return requested_.StartsWith(path);
    }

    template <class TValue>
    void operator()(const TValue& value, const TAbilitySourcePath& path)
    {
        if constexpr (std::is_same_v<TSource, std::remove_cvref_t<TValue>>) {
            if (!requested_.StartsWith(path)) {
                return;
            }
            const auto [existing, inserted] = seen_.emplace(path, &value);
            if (!inserted && existing->second != &value) {
                throw std::invalid_argument("ambiguous ability source provider path");
            }
            if (!located_ || path.StartsWith(located_->Path)) {
                located_ = TLocatedAbilitySource<TSource>{&value, path};
            }
        }
    }

    const std::optional<TLocatedAbilitySource<TSource>>& Get() const
    {
        return located_;
    }

private:
    TAbilitySourcePath requested_;
    std::map<TAbilitySourcePath, const TSource*> seen_;
    std::optional<TLocatedAbilitySource<TSource>> located_;
};

template <class TSource, class TOwner>
std::optional<TLocatedAbilitySource<TSource>> FindAbilitySource(
    const TOwner& owner, const TAbilitySourcePath& source)
{
    TAbilitySourceFinder<TSource> finder(source);
    owner.VisitAbilitySources(finder);
    return finder.Get();
}

#pragma once

#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/common/ability_source.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <vector>

void ValidateGeneratedSourceActor(const TGeneratedEntityView& owner);
const TAbilityRules& GeneratedSourceRules(const std::shared_ptr<TActionContext>& ctx,
                                         const TGeneratedEntityView& owner);

struct TGeneratedSourceKey {
    TEntityId OwnerId;
    const TPlayer* OwnerIdentity = nullptr;
    std::type_index OwnerType = typeid(void);
    std::type_index SourceType = typeid(void);
    TAbilitySourcePath Path;

    friend bool operator==(const TGeneratedSourceKey&, const TGeneratedSourceKey&) = default;
};

class TGeneratedSourceView {
public:
    TGeneratedSourceView(std::type_index owner_type, std::type_index source_type,
                         std::shared_ptr<const void> value, std::function<void()> validate,
                         FActionInputValidator validate_context = {},
                         std::optional<TGeneratedSourceKey> key = {});
    void Validate() const;
    void Validate(const std::shared_ptr<TActionContext>& ctx) const;
    const TGeneratedSourceKey& Key() const;
    void RequireTypes(std::type_index owner_type, std::type_index source_type) const;

    template <class T>
    const T& Read() const
    {
        if (source_type_ != typeid(T)) {
            throw std::invalid_argument("prepared ability source has a different type");
        }
        Validate();
        return *static_cast<const T*>(value_.get());
    }

private:
    std::type_index owner_type_;
    std::type_index source_type_;
    std::shared_ptr<const void> value_;
    std::function<void()> validate_;
    FActionInputValidator validate_context_;
    std::optional<TGeneratedSourceKey> key_;
};

template <class TSource>
class TGeneratedSourceBinding {
public:
    explicit TGeneratedSourceBinding(std::shared_ptr<const TGeneratedSourceView> view)
        : view_(std::move(view))
    {
        if (!view_) {
            throw TAbilityUnavailable();
        }
    }

    const TSource& Read() const
    {
        return view_->Read<TSource>();
    }

    void Validate() const
    {
        view_->Validate();
    }

    void Validate(const std::shared_ptr<TActionContext>& ctx) const
    {
        view_->Validate(ctx);
    }

    const TGeneratedSourceKey& Key() const
    {
        return view_->Key();
    }

    FActionInputValidator InputValidator() const
    {
        return [view = view_](const std::shared_ptr<TActionContext>& ctx) {
            view->Validate(ctx);
        };
    }

private:
    std::shared_ptr<const TGeneratedSourceView> view_;
};

template <class TSource>
class TGeneratedSourceCandidate {
public:
    using FResolve = std::function<const TSource&()>;
    using FDescribe = std::function<std::string(const TSource&)>;

    TGeneratedSourceCandidate(TGeneratedEntityView owner, TGeneratedSourceKey key,
                               FResolve resolve, FDescribe describe)
        : owner_(std::move(owner))
        , key_(std::move(key))
        , resolve_(std::move(resolve))
        , describe_(std::move(describe))
    {
        if (!resolve_ || !describe_ || key_.SourceType != typeid(TSource)) {
            throw std::invalid_argument("source candidate requires a typed locator and label");
        }
        owner_.Validate();
    }

    const TSource& Read() const
    {
        owner_.Validate();
        return resolve_();
    }

    void Validate(const std::shared_ptr<TActionContext>& ctx) const
    {
        ValidateActionEntityMembership(ctx, owner_);
        (void)resolve_();
    }

    const TGeneratedSourceKey& Key() const
    {
        return key_;
    }

    std::string Description() const
    {
        return describe_(Read());
    }

    TGeneratedSourceBinding<TSource> Snapshot(const std::shared_ptr<TActionContext>& ctx) const
    {
        Validate(ctx);
        auto value = std::make_shared<const TSource>(Read());
        auto view = std::make_shared<const TGeneratedSourceView>(
            key_.OwnerType, typeid(TSource), std::move(value),
            [candidate = *this] { (void)candidate.Read(); },
            [candidate = *this](const std::shared_ptr<TActionContext>& context) {
                candidate.Validate(context);
            }, key_);
        return TGeneratedSourceBinding<TSource>(std::move(view));
    }

private:
    TGeneratedEntityView owner_;
    TGeneratedSourceKey key_;
    FResolve resolve_;
    FDescribe describe_;
};

template <class TOwner, class TSource, class TGetter>
std::vector<TGeneratedSourceCandidate<TSource>> SourceCandidates(
    const TGeneratedEntityBinding<TOwner>& owner, const TAbilitySourcePath& collection_path,
    TGetter getter, typename TGeneratedSourceCandidate<TSource>::FDescribe describe = {})
{
    using TCollection = std::invoke_result_t<TGetter, const TOwner&>;
    static_assert(std::is_lvalue_reference_v<TCollection>);
    static_assert(std::is_same_v<std::remove_cvref_t<TCollection>, TIdCollection<TSource>>);
    static_assert(std::is_copy_constructible_v<TSource>);
    const auto readonly_owner = owner.ReadOnly();
    const auto& collection = std::invoke(getter, readonly_owner.Read());
    std::vector<TGeneratedSourceCandidate<TSource>> result;
    std::size_t ordinal = 0;
    for (const auto entry : collection) {
        const auto id = entry.Id();
        const TGeneratedSourceKey key{
            readonly_owner.Read().Id(), &readonly_owner.View().Player(),
            typeid(TOwner), typeid(TSource), collection_path.Entry(id.Value),
        };
        auto resolve = [readonly_owner, getter, id]() -> const TSource& {
            const auto& current = std::invoke(getter, readonly_owner.Read());
            const auto* value = current.Find(id);
            if (value == nullptr) {
                throw TAbilityUnavailable();
            }
            return *value;
        };
        ++ordinal;
        auto label = describe;
        if (!label) {
            label = [name = "Source " + std::to_string(ordinal)](const TSource&) { return name; };
        }
        result.emplace_back(readonly_owner.View(), key, std::move(resolve), std::move(label));
    }
    return result;
}

template <class TOwner, class TSource, class TGetter>
std::vector<TGeneratedSourceCandidate<TSource>> SourceCandidates(
    const std::shared_ptr<TActionContext>& ctx, const TGeneratedEntityBinding<TOwner>& owner,
    const TAbilitySourcePath& collection_path, TGetter getter,
    typename TGeneratedSourceCandidate<TSource>::FDescribe describe = {})
{
    ValidateActionActor(ctx);
    ValidateActionEntityMembership(ctx, owner.View());
    return SourceCandidates<TOwner, TSource>(owner, collection_path, std::move(getter),
                                             std::move(describe));
}

template <class TSource>
std::vector<TGeneratedSourceCandidate<TSource>> MergeSourceCandidates(
    std::vector<std::vector<TGeneratedSourceCandidate<TSource>>> groups)
{
    std::vector<TGeneratedSourceCandidate<TSource>> result;
    for (auto& group : groups) {
        for (auto& candidate : group) {
            if (!std::ranges::any_of(result, [&](const auto& existing) {
                    return existing.Key() == candidate.Key();
                })) {
                result.push_back(std::move(candidate));
            }
        }
    }
    return result;
}

template <class TSource>
TGeneratedSourceBinding<TSource> SelectActionSource(
    const std::shared_ptr<TActionContext>& ctx,
    std::vector<TGeneratedSourceCandidate<TSource>> candidates,
    std::function<bool(const TGeneratedSourceCandidate<TSource>&)> predicate,
    std::function<void(TGeneratedSourceBinding<TSource>)> on_resume)
{
    ValidateActionActor(ctx);
    if (!on_resume) {
        throw std::invalid_argument("source choice requires a continuation");
    }
    std::vector<TGeneratedSourceCandidate<TSource>> eligible;
    std::vector<std::string> descriptions;
    std::map<std::string, std::size_t> totals;
    for (const auto& candidate : candidates) {
        candidate.Validate(ctx);
        if (!predicate || predicate(candidate)) {
            descriptions.push_back(candidate.Description());
            ++totals[descriptions.back()];
            eligible.push_back(candidate);
        }
    }
    auto values = std::make_shared<const std::vector<TGeneratedSourceCandidate<TSource>>>(
        std::move(eligible));
    auto alternatives = TAlternatives::Create<TGeneratedSourceCandidate<TSource>>("source");
    std::map<std::string, std::size_t> occurrences;
    std::set<std::string> used_labels;
    for (std::size_t index = 0; index < values->size(); ++index) {
        auto label = descriptions[index];
        if (totals[label] > 1) {
            label += " (" + std::to_string(++occurrences[descriptions[index]]) + ")";
        }
        while (!used_labels.insert(label).second) {
            label = descriptions[index] + " (" +
                std::to_string(++occurrences[descriptions[index]]) + ")";
        }
        alternatives.AddAlternative(std::move(label), (*values)[index]);
    }
    auto selected = [ctx, values, predicate = std::move(predicate)](std::size_t index) {
        ValidateActionActor(ctx);
        if (index >= values->size()) {
            throw std::out_of_range("selected source is outside the alternatives");
        }
        const auto& candidate = (*values)[index];
        candidate.Validate(ctx);
        if (predicate && !predicate(candidate)) {
            throw TAbilityUnavailable();
        }
        return candidate.Snapshot(ctx);
    };
    const auto index = SelectActionIndex(ctx, std::move(alternatives),
        [selected, on_resume = std::move(on_resume)](std::size_t choice) {
            on_resume(selected(choice));
        });
    return selected(index);
}

template <class TOwner, class TSource>
bool RequiredSource(const TPlayer& actor, const TAbilitySourcePath& source)
{
    const auto owner = BindActionEntity<TOwner>(actor);
    return FindAbilitySource<TSource>(owner.Read(), source).has_value();
}

template <class TOwner, class TSource>
void PrepareActionSource(const std::shared_ptr<TActionContext>& ctx, TPlayer& actor,
                          const TGrantedAbility& grant)
{
    if (!ctx) {
        throw std::invalid_argument("ability source preparation requires a context");
    }
    const auto owner = TGeneratedEntityBinding<TOwner>(
        BindGeneratedActionEntity(std::as_const(actor), ctx->battle));
    const auto& rules = GeneratedSourceRules(ctx, owner.View());
    const auto located = FindAbilitySource<TSource>(owner.Read(), grant.Source);
    if (!located) {
        throw TAbilityUnavailable();
    }
    auto validate = [owner, catalog = &rules.Catalog, definition = grant.DefinitionId,
                     source = grant.Source, provider = located->Path,
                     previous = ctx->validate_ability]() {
        ValidateGeneratedSourceActor(owner.View());
        if (previous && !previous()) {
            throw TAbilityUnavailable();
        }
        const auto current_source = FindAbilitySource<TSource>(owner.Read(), source);
        if (!current_source || current_source->Path != provider) {
            throw TAbilityUnavailable();
        }
        const auto abilities = CollectAbilities(owner.Read(), *catalog);
        if (!std::ranges::any_of(abilities.Grants(), [&](const TGrantedAbility& current) {
                return current.DefinitionId == definition && current.Source == source;
            })) {
            throw TAbilityUnavailable();
        }
    };
    validate();
    auto snapshot = std::make_shared<const TSource>(*located->Value);
    auto view = std::make_shared<const TGeneratedSourceView>(
        typeid(TOwner), typeid(TSource), std::move(snapshot), std::move(validate),
        [owner](const std::shared_ptr<TActionContext>& context) {
            ValidateActionEntityMembership(context, owner.View());
        }, TGeneratedSourceKey{owner.Read().Id(), &owner.View().Player(), typeid(TOwner),
                               typeid(TSource), located->Path});
    ctx->validate_ability = [view]() {
        view->Validate();
        return true;
    };
    ctx->generated_source = std::move(view);
}

template <class TOwner, class TSource>
TGeneratedSourceBinding<TSource> ActionSource(const std::shared_ptr<TActionContext>& ctx)
{
    if (!ctx || !ctx->generated_source) {
        throw std::invalid_argument("action requires a prepared ability source");
    }
    ctx->generated_source->RequireTypes(typeid(TOwner), typeid(TSource));
    ctx->generated_source->Validate();
    return TGeneratedSourceBinding<TSource>(ctx->generated_source);
}

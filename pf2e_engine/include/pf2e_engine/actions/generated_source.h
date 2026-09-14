#pragma once

#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/common/ability_source.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <utility>

void ValidateGeneratedSourceActor(const TGeneratedEntityView& owner);
const TAbilityRules& GeneratedSourceRules(const std::shared_ptr<TActionContext>& ctx,
                                         const TGeneratedEntityView& owner);

class TGeneratedSourceView {
public:
    TGeneratedSourceView(std::type_index owner_type, std::type_index source_type,
                         std::shared_ptr<const void> value, std::function<void()> validate);
    void Validate() const;
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

private:
    std::shared_ptr<const TGeneratedSourceView> view_;
};

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
        typeid(TOwner), typeid(TSource), std::move(snapshot), std::move(validate));
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

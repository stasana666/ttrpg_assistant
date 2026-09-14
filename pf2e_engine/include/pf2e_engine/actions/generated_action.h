#pragma once

#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/common/entity_id.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/transformation/transformator.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <utility>
#include <vector>

class TGeneratedEntityView;
TGeneratedEntityView BindGeneratedActionEntity(const TPlayer& player, const TBattle* battle = nullptr);
TGeneratedEntityView BindGeneratedActionEntity(TPlayer& player, const TBattle* battle = nullptr);
TGeneratedEntityView GeneratedActionSelf(const std::shared_ptr<TActionContext>& ctx);
void ValidateActionActor(const std::shared_ptr<TActionContext>& ctx);
void ValidateActionEntity(const std::shared_ptr<TActionContext>& ctx,
                          const TGeneratedEntityView& entity);
const TAbilityRules& GeneratedActionRules(const std::shared_ptr<TActionContext>& ctx);
std::vector<TGeneratedEntityView> GeneratedActionEntities(const std::shared_ptr<TActionContext>& ctx,
                                                        std::type_index type);
void RequestActionEntity(const std::shared_ptr<TActionContext>& ctx,
                         std::vector<TGeneratedEntityView> candidates,
                         std::function<void(TGeneratedEntityView)> receive);
int RollActionDice(const std::shared_ptr<TActionContext>& ctx, int count, int size);

class TGeneratedEntityView {
public:
    TGeneratedEntityView() = default;
    void Validate() const;
    const TPlayer& Player() const;
    TPlayer& MutablePlayer() const;
    TGeneratedEntityView ReadOnly() const;

    friend bool operator==(const TGeneratedEntityView& lhs, const TGeneratedEntityView& rhs)
    {
        return lhs.type_ == rhs.type_ && lhs.id_ == rhs.id_ && lhs.address_ == rhs.address_;
    }

private:
    template <class T>
    friend class TGeneratedEntityBinding;
    friend TGeneratedEntityView BindGeneratedActionEntity(const TPlayer&, const TBattle*);
    friend TGeneratedEntityView BindGeneratedActionEntity(TPlayer&, const TBattle*);

    std::type_index type_ = typeid(void);
    TEntityId id_;
    const void* address_ = nullptr;
    void* mutable_address_ = nullptr;
    const TPlayer* player_ = nullptr;
    TPlayer* mutable_player_ = nullptr;
    const TBattle* battle_ = nullptr;
};

template <class T, class Getter>
auto ProjectActionField(TGuarded<T> target, Getter getter)
{
    return TGeneratedExecutionContext::Project(target, std::move(getter));
}

template <class T>
class TGeneratedEntityBinding {
public:
    TGeneratedEntityBinding() = default;
    explicit TGeneratedEntityBinding(TGeneratedEntityView view)
        : view_(std::move(view))
    {
        if (view_.type_ != typeid(T)) {
            throw std::invalid_argument("action entity type has no matching runtime binding");
        }
        view_.Validate();
    }

    const T& Read() const
    {
        view_.Validate();
        return *static_cast<const T*>(view_.address_);
    }

    TGuarded<T> Guard() const
    {
        view_.Validate();
        if (view_.mutable_address_ == nullptr) {
            throw std::logic_error("read-only entity binding cannot be mutated");
        }
        return TGuarded<T>(*static_cast<T*>(view_.mutable_address_));
    }

    template <class Getter>
    auto Project(Getter getter) const
    {
        return ProjectActionField(Guard(), std::move(getter));
    }

    const TGeneratedEntityView& View() const
    {
        return view_;
    }

    TGeneratedEntityBinding ReadOnly() const
    {
        return TGeneratedEntityBinding(view_.ReadOnly());
    }

    friend bool operator==(const TGeneratedEntityBinding&, const TGeneratedEntityBinding&) = default;

private:
    TGeneratedEntityView view_;
};

template <class T>
TGeneratedEntityBinding<T> BindActionEntity(const TPlayer& player)
{
    return TGeneratedEntityBinding<T>(BindGeneratedActionEntity(player));
}

template <class T>
TGeneratedEntityBinding<T> BindActionEntity(TPlayer& player)
{
    return TGeneratedEntityBinding<T>(BindGeneratedActionEntity(player));
}

template <class T>
TGeneratedEntityBinding<T> ActionSelf(const std::shared_ptr<TActionContext>& ctx)
{
    return TGeneratedEntityBinding<T>(GeneratedActionSelf(ctx));
}

template <class T>
std::vector<TGeneratedEntityBinding<T>> ActionEntities(const std::shared_ptr<TActionContext>& ctx)
{
    std::vector<TGeneratedEntityBinding<T>> bindings;
    for (auto view : GeneratedActionEntities(ctx, typeid(T))) {
        bindings.emplace_back(std::move(view));
    }
    return bindings;
}

template <class T>
void ChooseActionEntity(const std::shared_ptr<TActionContext>& ctx,
                        std::vector<TGeneratedEntityBinding<T>> candidates,
                        std::function<bool(const TGeneratedEntityBinding<T>&)> predicate,
                        std::function<void(TGeneratedEntityBinding<T>)> receive)
{
    if (!receive) {
        throw std::invalid_argument("entity choice requires a receiver");
    }
    std::vector<TGeneratedEntityView> eligible;
    for (const auto& candidate : candidates) {
        candidate.View().Validate();
        if (!predicate || predicate(candidate)) {
            eligible.push_back(candidate.View());
        }
    }
    RequestActionEntity(ctx, std::move(eligible),
        [predicate = std::move(predicate), receive = std::move(receive)](TGeneratedEntityView view) {
            TGeneratedEntityBinding<T> selected(std::move(view));
            if (predicate && !predicate(selected)) {
                throw TAbilityUnavailable();
            }
            receive(std::move(selected));
        });
}

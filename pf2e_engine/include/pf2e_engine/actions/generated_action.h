#pragma once

#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/alternatives.h>
#include <pf2e_engine/common/entity_id.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <vector>

class TGeneratedEntityView;
TGeneratedEntityView BindGeneratedActionEntity(const TPlayer& player, const TBattle* battle = nullptr);
TGeneratedEntityView BindGeneratedActionEntity(TPlayer& player, const TBattle* battle = nullptr);
TGeneratedEntityView GeneratedActionSelf(const std::shared_ptr<TActionContext>& ctx);
void ValidateActionActor(const std::shared_ptr<TActionContext>& ctx);
void ValidateActionActorBinding(const std::shared_ptr<TActionContext>& ctx,
                                const TGeneratedEntityView& actor);
void ValidateActionEntity(const std::shared_ptr<TActionContext>& ctx,
                          const TGeneratedEntityView& entity);
void ValidateActionEntityMembership(const std::shared_ptr<TActionContext>& ctx,
                                    const TGeneratedEntityView& entity);
void ValidateActionInputs(const std::shared_ptr<TActionContext>& ctx);
std::shared_ptr<TActionContext> ForkActionInputs(
    const std::shared_ptr<TActionContext>& ctx, std::vector<FActionInputValidator> validators);
const TAbilityRules& GeneratedActionRules(const std::shared_ptr<TActionContext>& ctx);
std::vector<TGeneratedEntityView> GeneratedActionEntities(const std::shared_ptr<TActionContext>& ctx,
                                                        std::type_index type);
int RollActionDice(const std::shared_ptr<TActionContext>& ctx, int count, int size);
std::size_t SelectActionIndex(const std::shared_ptr<TActionContext>& ctx,
                              TAlternatives alternatives,
                              std::function<void(std::size_t)> on_resume);
TGeneratedEntityView MutableSelectedActionEntity(const std::shared_ptr<TActionContext>& ctx,
                                                 const TGeneratedEntityView& selected);

class TGeneratedEntityView {
public:
    TGeneratedEntityView() = default;
    void Validate() const;
    const TPlayer& Player() const;
    TPlayer& MutablePlayer() const;
    TGeneratedEntityView ReadOnly() const;
    std::type_index Type() const
    {
        Validate();
        return type_;
    }

    friend bool operator==(const TGeneratedEntityView& lhs, const TGeneratedEntityView& rhs)
    {
        return lhs.type_ == rhs.type_ && lhs.id_ == rhs.id_ && lhs.address_ == rhs.address_;
    }

private:
    template <class T>
    friend class TGeneratedEntityBinding;
    friend TGeneratedEntityView BindGeneratedActionEntity(const TPlayer&, const TBattle*);
    friend TGeneratedEntityView BindGeneratedActionEntity(TPlayer&, const TBattle*);
    friend void ValidateActionEntityMembership(const std::shared_ptr<TActionContext>&,
                                               const TGeneratedEntityView&);

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
T SelectActionValue(const std::shared_ptr<TActionContext>& ctx, std::vector<T> candidates,
                     std::function<bool(const T&)> predicate, std::function<void(T)> on_resume)
{
    ValidateActionActor(ctx);
    if (!on_resume) {
        throw std::invalid_argument("value choice requires a continuation");
    }
    std::vector<T> eligible;
    for (const auto& candidate : candidates) {
        if (!predicate || predicate(candidate)) {
            eligible.push_back(candidate);
        }
    }
    auto values = std::make_shared<const std::vector<T>>(std::move(eligible));
    auto alternatives = TAlternatives::Create<T>(
        std::is_same_v<T, TPosition> ? "move to position" : "choice");
    for (const auto& value : *values) {
        std::string description;
        if constexpr (std::is_same_v<T, TPosition>) {
            description = std::to_string(value.x) + " " + std::to_string(value.y);
        } else if constexpr (std::is_same_v<T, std::string>) {
            description = value;
        } else if constexpr (std::is_same_v<T, bool>) {
            description = value ? "true" : "false";
        } else if constexpr (std::is_integral_v<T>) {
            description = std::to_string(value);
        } else {
            description = ToString(value);
        }
        alternatives.AddAlternative(std::move(description), T(value));
    }
    auto selected = [ctx, values, predicate = std::move(predicate)](std::size_t index) -> T {
        ValidateActionActor(ctx);
        if (index >= values->size()) {
            throw std::out_of_range("selected value is outside the alternatives");
        }
        T value = (*values)[index];
        if (predicate && !predicate(value)) {
            throw TAbilityUnavailable();
        }
        return value;
    };
    const auto index = SelectActionIndex(ctx, std::move(alternatives),
        [selected, on_resume = std::move(on_resume)](std::size_t choice) {
            on_resume(selected(choice));
        });
    return selected(index);
}

template <class T>
TGeneratedEntityBinding<T> SelectActionEntity(
    const std::shared_ptr<TActionContext>& ctx, std::vector<TGeneratedEntityBinding<T>> candidates,
    std::function<bool(const TGeneratedEntityBinding<T>&)> predicate,
    std::function<void(TGeneratedEntityBinding<T>)> on_resume)
{
    ValidateActionActor(ctx);
    if (!on_resume) {
        throw std::invalid_argument("entity choice requires a continuation");
    }
    std::vector<TGeneratedEntityBinding<T>> eligible;
    auto alternatives = TAlternatives::Create<TPlayer*>("target");
    for (const auto& candidate : candidates) {
        ValidateActionEntity(ctx, candidate.View());
        if (!predicate || predicate(candidate)) {
            const auto mutable_view = MutableSelectedActionEntity(ctx, candidate.View());
            alternatives.AddAlternative(std::string(mutable_view.Player().GetName()),
                                         &mutable_view.MutablePlayer());
            eligible.push_back(candidate.ReadOnly());
        }
    }
    auto values = std::make_shared<const std::vector<TGeneratedEntityBinding<T>>>(std::move(eligible));
    auto selected = [ctx, values, predicate = std::move(predicate)](std::size_t index) {
        ValidateActionActor(ctx);
        if (index >= values->size()) {
            throw std::out_of_range("selected entity is outside the alternatives");
        }
        const auto& value = (*values)[index];
        ValidateActionEntity(ctx, value.View());
        if (predicate && !predicate(value)) {
            throw TAbilityUnavailable();
        }
        return TGeneratedEntityBinding<T>(MutableSelectedActionEntity(ctx, value.View()));
    };
    const auto index = SelectActionIndex(ctx, std::move(alternatives),
        [selected, on_resume = std::move(on_resume)](std::size_t choice) {
            on_resume(selected(choice));
        });
    return selected(index);
}

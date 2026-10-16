#pragma once

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/guarded.h>
#include <pf2e_engine/common/id_collection.h>
#include <pf2e_engine/common/resource.h>
#include <pf2e_engine/common/rule_integer.h>
#include <pf2e_engine/common/variant_map.h>
#include <pf2e_engine/transformation/state.h>

#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

class TTransformator;
class TGeneratedExecutionContext;
class TGeneratedTransaction;

namespace generated_assignment_detail {

template <class T>
void AddPrevious(TAstNode& node, std::string_view label, const T& value, TAstContext& ctx)
{
    if constexpr (TIsAstRecursive<T>::value) {
        AddOwnedObject(node, label, value, ctx);
    } else {
        AddValueField(node, label, value);
    }
}

template <class T>
void AddPrevious(TAstNode& node, std::string_view label, const TIdCollection<T>& value,
                 TAstContext& ctx)
{
    auto collection = TAstNode::MakeObject("collection");
    AddValueField(collection, "next_id", value.NextId().Value);
    for (const auto entry : value) {
        AddPrevious(collection, std::to_string(entry.Id().Value), *entry, ctx);
    }
    node.AddChild(label, std::move(collection));
}

template <class TKind, class TVariant>
void AddPrevious(TAstNode& node, std::string_view label,
                 const TVariantMap<TKind, TVariant>& value, TAstContext& ctx)
{
    auto variants = TAstNode::MakeObject("container");
    for (const auto& [kind, variant] : value) {
        AddPrevious(variants, ToString(kind), variant, ctx);
    }
    node.AddChild(label, std::move(variants));
}

}

class TGeneratedAssignment {
public:
    void Undo() noexcept;
    TAstNode GetAst(TAstContext& ctx) const;

private:
    friend class TGeneratedExecutionContext;

    class IValue {
    public:
        virtual ~IValue() = default;
        virtual void Swap() noexcept = 0;
        virtual TAstNode GetAst(TAstContext& ctx) const = 0;
    };

    template <class T>
    class TValue final : public IValue {
    public:
        TValue(T& target, T value, const void* owner, std::string field)
            : target_(&target)
            , previous_(std::move(value))
            , owner_(owner)
            , field_(std::move(field))
        {
        }

        void Swap() noexcept override
        {
            using std::swap;
            swap(*target_, previous_);
        }

        TAstNode GetAst(TAstContext& ctx) const override
        {
            TAstNode node = TAstNode::MakeObject("TGeneratedAssignment");
            AddReference(node, "owner_ref", owner_, ctx);
            AddValueField(node, "field", field_);
            generated_assignment_detail::AddPrevious(node, "previous", previous_, ctx);
            return node;
        }

    private:
        T* target_;
        T previous_;
        const void* owner_;
        std::string field_;
    };

    explicit TGeneratedAssignment(std::unique_ptr<IValue> value);
    void Apply() noexcept;

    std::unique_ptr<IValue> value_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

class TGeneratedExecutionContext {
public:
    explicit TGeneratedExecutionContext(TTransformator& transformator);

    template <class T>
        requires std::copy_constructible<T> && std::is_nothrow_swappable_v<T>
    void Assign(T& target, std::type_identity_t<T> value, const void* owner,
                std::string_view field)
    {
        ValidateField(owner, field);
        RecordAndApply(TGeneratedAssignment(
            std::make_unique<TGeneratedAssignment::TValue<T>>(
                target, std::move(value), owner, std::string(field))));
    }

    template <class T, class Method, class... Args>
    decltype(auto) Invoke(TGuarded<T> target, Method method, Args&&... args)
    {
        return std::invoke(method, target.Mutable(), *this, std::forward<Args>(args)...);
    }

    template <class T, class Getter>
    static auto Project(TGuarded<T> target, Getter getter)
    {
        return RequireGuarded(std::invoke(getter, target.Mutable()));
    }

    template <class T, class Owner>
        requires std::copy_constructible<T> && std::is_nothrow_swappable_v<T>
    void AssignGuarded(TGuarded<T> target, std::type_identity_t<T> value,
                       TGuarded<Owner> owner, std::string_view field)
    {
        Assign(target.Mutable(), std::move(value), &owner.Get(), field);
    }

    template <class Kind, class Variant, class Owner>
    void SetVariant(TGuarded<TVariantMap<Kind, Variant>> target, Variant value,
                     TGuarded<Owner> owner, std::string_view field)
    {
        auto changed = target.Get();
        changed.Set(std::move(value));
        AssignGuarded(target, std::move(changed), owner, field);
    }

    template <class Kind, class Variant, class Owner>
    void EraseVariant(TGuarded<TVariantMap<Kind, Variant>> target, Kind kind,
                       TGuarded<Owner> owner, std::string_view field)
    {
        auto changed = target.Get();
        changed.Erase(kind);
        AssignGuarded(target, std::move(changed), owner, field);
    }

    template <class Owner>
    void AddResource(TGuarded<TResource> target, int count,
                      TGuarded<Owner> owner, std::string_view field)
    {
        RequireResourceCount(target.Get().Count());
        RequireResourceCount(count);
        AssignGuarded(target, TResource(TRuleInteger::Add(target.Get().Count(), count)), owner, field);
    }

    template <class Owner>
    void SpendResource(TGuarded<TResource> target, int count,
                        TGuarded<Owner> owner, std::string_view field)
    {
        RequireResourceCount(target.Get().Count());
        RequireResourceCount(count);
        if (target.Get().Count() < count) {
            throw std::invalid_argument("insufficient resource count");
        }
        AssignGuarded(target, TResource(target.Get().Count() - count), owner, field);
    }

    template <class Owner>
    void SetResourceCount(TGuarded<TResource> target, int count,
                           TGuarded<Owner> owner, std::string_view field)
    {
        RequireResourceCount(target.Get().Count());
        RequireResourceCount(count);
        AssignGuarded(target, TResource(count), owner, field);
    }

    TState GetState() const;
    void Undo(TState state) const;
    [[nodiscard]] TGeneratedTransaction Atomic() const;

private:
    static void RequireResourceCount(int count)
    {
        if (count < 0) {
            throw std::invalid_argument("resource count cannot be negative");
        }
    }

    template <class T>
    static TGuarded<T> RequireGuarded(TGuarded<T> value)
    {
        return value;
    }

    static void ValidateField(const void* owner, std::string_view field);
    void RecordAndApply(TGeneratedAssignment assignment);

    TTransformator* transformator_;
};

class TGeneratedTransaction {
public:
    TGeneratedTransaction(const TGeneratedTransaction&) = delete;
    TGeneratedTransaction& operator=(const TGeneratedTransaction&) = delete;
    ~TGeneratedTransaction() noexcept;

    void Commit() noexcept;

private:
    friend class TGeneratedExecutionContext;

    explicit TGeneratedTransaction(TGeneratedExecutionContext context);

    TGeneratedExecutionContext context_;
    TState state_;
    bool committed_ = false;
};

template <>
struct TIsAstRecursive<TGeneratedAssignment> : std::true_type {};

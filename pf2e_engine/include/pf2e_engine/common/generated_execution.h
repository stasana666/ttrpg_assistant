#pragma once

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/guarded.h>
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
            if constexpr (TIsAstRecursive<T>::value) {
                AddOwnedObject(node, "previous", previous_, ctx);
            } else {
                AddValueField(node, "previous", previous_);
            }
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

    TState GetState() const;
    void Undo(TState state) const;
    [[nodiscard]] TGeneratedTransaction Atomic() const;

private:
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

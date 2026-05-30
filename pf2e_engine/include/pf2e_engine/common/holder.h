#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/ast/ast_node.h>

#include <cassert>
#include <memory>
#include <mutex>

template <class T>
class THolder {
public:
    explicit THolder(T&& value)
        : value_(std::make_shared<T>(std::move(value)))
    {
    }

    std::shared_ptr<const T> Get() const
    {
        std::lock_guard lock(mutex_);
        std::shared_ptr<T> copy = value_;
        return copy;
    }

    std::shared_ptr<T> Copy()
    {
        std::shared_ptr<T> copy;
        {
            std::lock_guard lock(mutex_);
            copy = value_;
        }
        return std::make_shared<T>(*copy);
    }

    void Set(std::shared_ptr<T> value)
    {
        assert(value != nullptr);
        std::lock_guard lock(mutex_);
        value_ = value;
    }

    TAstNode GetAst(TAstContext& ctx) const
    {
        std::shared_ptr<const T> snapshot;
        {
            std::lock_guard lock(mutex_);
            snapshot = value_;
        }
        if (!snapshot) {
            return TAstNode::MakeNull();
        }
        return snapshot->GetAst(ctx);
    }

private:
    std::shared_ptr<T> value_;
    mutable std::mutex mutex_;
};

template <class T>
struct TIsAstRecursive<THolder<T>> : TIsAstRecursive<T> {};

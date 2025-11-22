#pragma once

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

private:
    std::shared_ptr<T> value_;
    mutable std::mutex mutex_;
};

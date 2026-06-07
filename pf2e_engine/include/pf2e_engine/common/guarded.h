#pragma once

template <class T>
class TGuarded {
public:
    explicit TGuarded(T& v) : v_(&v) {}

    const T& Get() const { return *v_; }
    operator const T&() const { return *v_; }
    const T* operator->() const { return v_; }
    const T& operator*() const { return *v_; }

private:
    friend class TTransformator;
    friend class TChangeCondition;
    T& Mutable() const { return *v_; }

    T* v_;
};

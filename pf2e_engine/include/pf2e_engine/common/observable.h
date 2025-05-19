#pragma once

#include <functional>

template <typename Context>
class TObservable {
public:
    using TCallback = std::function<void(Context)>;

public:
    void Subscribe(TCallback callback);
    void NotifyAll(Context context) const;

private:
    std::vector<TCallback> subscribers;
};

template <typename Context>
void TObservable<Context>::Subscribe(TCallback callback) {
    subscribers.push_back(callback);
}

template <typename Context>
void TObservable<Context>::NotifyAll(Context context) const {
    for (const auto& subscriber : subscribers) {
        subscriber(context);
    }
}

#pragma once

#include <pf2e_engine/player.h>

#include <list>

enum class EEvent {
    OnRoundStart,
    OnRoundEnd,
    OnTurnStart,
    OnTurnEnd,
};

struct TEventContext {
    TPlayer* player;

    bool operator ==(const TEventContext& other) const = default;
};

struct TEvent {
    EEvent type;
    TEventContext context;

    bool operator ==(const TEvent& other) const = default;
};

struct TTask {
    std::vector<TEvent> events_before_call;
    std::function<void()> callback;
};

class TTaskScheduler {
public:
    void TriggerEvent(TEvent event);

    void AddTask(TTask&& task);

private:
    std::list<TTask> tasks_;
};

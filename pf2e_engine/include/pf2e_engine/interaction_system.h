#pragma once

#include <pf2e_engine/alternatives.h>
#include <pf2e_engine/gui/click_event.h>
#include <pf2e_engine/position.h>
#include <pf2e_engine/common/channel.h>

#include <cassert>
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

class TInteractionSystem {
public:
    explicit TInteractionSystem(TChannel<TClickEvent>::TConsumer click_queue);
    ~TInteractionSystem();

    void CinReaderWorker();

    template <class T>
    T ChooseAlternative(int player_id, const TAlternatives<T>& alternatives);

    std::ostream& GameLog();
    std::ostream& DevLog();

private:
    struct TCinEvent {
        using Timepoint = std::chrono::time_point<std::chrono::steady_clock>;
        int value;
        Timepoint timepoint;
    };

    std::thread cin_reader_;
    TChannel<TCinEvent> cin_queue_;
    TChannel<TClickEvent>::TConsumer click_queue_;
};

template <class T>
T TInteractionSystem::ChooseAlternative(
    int player_id,
    const TAlternatives<T>& alternatives)
{
    assert(!alternatives.Empty());
    if (alternatives.Size() == 1) {
        return alternatives[0].value;
    }

    std::stringstream question;
    question << "Ask " << player_id << "\n";
    question << "Choose " << alternatives.Kind() << " and write it's number:" << std::endl;
    for (size_t i = 0; i < alternatives.Size(); ++i) {
        question << i << " - " << alternatives[i].name << std::endl;
    }

    bool has_progress{false};
    int result{-1};
    std::vector<std::function<bool(void)>> input_ways;
    TClickEvent::Timepoint now = std::chrono::steady_clock::now();

    std::cout << question.str();
    std::cout.flush();
    input_ways.emplace_back([&]() {
        TCinEvent event;
        if (!cin_queue_.Dequeue(event)) {
            return false;
        }
        has_progress = true;
        if (event.timepoint < now) {
            return false;
        }
        if (event.value >= static_cast<ssize_t>(alternatives.Size()) || event.value < 0) {
            std::cout << question.str();
            return false;
        }
        result = event.value;
        return true;
    });

    if constexpr (std::is_same_v<TPosition, T>) {
        input_ways.emplace_back([&]() {
            TClickEvent event;
            if (!click_queue_.Dequeue(event)) {
                return false;
            }
            std::cout << "Deque TClickEvent: x = " << event.position.x << ", y = " << event.position.y << std::endl;
            has_progress = true;
            if (event.timepoint < now) {
                return false;
            }
            for (size_t i = 0; i < alternatives.Size(); ++i) {
                if (alternatives[i].value == event.position) {
                    result = i;
                    return true;
                }
            }
            return false;
        });
    }

    // TODO: заменить spinlock на нормальное решение с condition_variable
    while (true) {
        has_progress = false;
        for (auto& way : input_ways) {
            if (way()) {
                return alternatives[result].value;
            }
        }
        if (!has_progress) {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    return alternatives[result].value;
}

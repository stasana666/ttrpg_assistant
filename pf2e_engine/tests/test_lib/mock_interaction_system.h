#pragma once

#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/common/visit.h>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <queue>
#include <variant>

struct TTooManyCallsError : public std::logic_error {
    explicit TTooManyCallsError(std::string message)
        : std::logic_error(message) {}
};

struct TMockInteractionSystem final : public IInteractionSystem {
    std::ostream& GameLog() override {
        return game_log_;
    }

    std::ostream& DevLog() override {
        return dev_log_;
    }

    struct ExpectedChoice {
        int player_id;
        std::string kind;
        std::string choice_name;
    };

    using CheckCallback = std::function<void()>;
    using Expectation = std::variant<ExpectedChoice, CheckCallback>;

protected:
    size_t ChooseAlternativeIndex(int player_id, const TAlternatives& alternatives) override {
        ExecutePendingCallbacks();

        if (expectations_.empty()) {
            throw TTooManyCallsError("TMockInteractionSystem: too many calls to ChooseAlternativeIndex");
        }

        size_t result = std::visit(VisitorHelper{
            [&](const ExpectedChoice& expected) -> size_t {
                if (player_id != expected.player_id) {
                    throw std::logic_error("TMockInteractionSystem: unexpected player_id. Expected: " +
                                           std::to_string(expected.player_id) + ", got: " +
                                           std::to_string(player_id));
                }

                if (std::string(alternatives.Kind()) != expected.kind) {
                    throw std::logic_error("TMockInteractionSystem: unexpected alternatives kind. Expected: " +
                                           expected.kind + ", got: " + std::string(alternatives.Kind()));
                }

                for (size_t i = 0; i < alternatives.Size(); ++i) {
                    if (alternatives[i].name == expected.choice_name) {
                        return i;
                    }
                }

                std::string available_names;
                for (size_t i = 0; i < alternatives.Size(); ++i) {
                    if (i > 0) {
                        available_names += ", ";
                    }
                    available_names += "\"" + alternatives[i].name + "\"";
                }
                throw std::logic_error("TMockInteractionSystem: choice name \"" + expected.choice_name +
                                       "\" not found in alternatives. Available: [" + available_names + "]");
            },
            [](const CheckCallback&) -> size_t {
                throw std::logic_error("TMockInteractionSystem: expected a choice but got callback");
            }
        }, expectations_.front());

        expectations_.pop();
        return result;
    }

public:
    void ExpectChoice(int player_id, std::string kind, std::string choice_name) {
        expectations_.push(ExpectedChoice{player_id, std::move(kind), std::move(choice_name)});
    }

    void AddCheckCallback(CheckCallback callback) {
        expectations_.push(std::move(callback));
    }

    void Verify() {
        ExecutePendingCallbacks();

        if (!expectations_.empty()) {
            throw std::logic_error("TMockInteractionSystem: expected choice was not made. Remaining: " +
                                   std::to_string(expectations_.size()));
        }
    }

    std::string GetGameLog() const {
        return game_log_.str();
    }

    std::string GetDevLog() const {
        return dev_log_.str();
    }

private:
    void ExecutePendingCallbacks() {
        while (!expectations_.empty()) {
            bool should_stop = std::visit(VisitorHelper{
                [](const ExpectedChoice&) { return true; },
                [this](const CheckCallback& callback) {
                    expectations_.pop();
                    callback();
                    return false;
                }
            }, expectations_.front());
            if (should_stop) {
                break;
            }
        }
    }

    std::queue<Expectation> expectations_;
    std::ostringstream game_log_;
    std::ostringstream dev_log_;
};

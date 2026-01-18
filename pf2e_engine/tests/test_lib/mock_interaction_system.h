#pragma once

#include <pf2e_engine/i_interaction_system.h>
#include <sstream>
#include <stdexcept>
#include <queue>

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

protected:
    size_t ChooseAlternativeIndex(int player_id, const TAlternatives& alternatives) override {
        if (expected_choices_.empty()) {
            throw TTooManyCallsError("TMockInteractionSystem: too many calls to ChooseAlternativeIndex");
        }

        auto expected = expected_choices_.front();
        expected_choices_.pop();

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
    }

public:
    struct ExpectedChoice {
        int player_id;
        std::string kind;
        std::string choice_name;
    };

    void ExpectChoice(int player_id, std::string kind, std::string choice_name) {
        expected_choices_.push({player_id, std::move(kind), std::move(choice_name)});
    }

    void Verify() {
        if (!expected_choices_.empty()) {
            throw std::logic_error("TMockInteractionSystem: expected choice was not made. Remaining: " +
                                   std::to_string(expected_choices_.size()));
        }
    }

    std::string GetGameLog() const {
        return game_log_.str();
    }

    std::string GetDevLog() const {
        return dev_log_.str();
    }

private:
    std::queue<ExpectedChoice> expected_choices_;
    std::ostringstream game_log_;
    std::ostringstream dev_log_;
};

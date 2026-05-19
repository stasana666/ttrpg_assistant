#pragma once

#include <analyzer/decision_strategy.h>

#include <pf2e_engine/i_interaction_system.h>

#include <ostream>
#include <streambuf>

// Headless IInteractionSystem for simulations: choices are made by a strategy
// and all log output is discarded.
class TAutomatedInteractionSystem : public IInteractionSystem {
public:
    explicit TAutomatedInteractionSystem(const IDecisionStrategy& strategy)
        : strategy_(strategy)
        , null_stream_(&null_buffer_)
    {
    }

    std::ostream& GameLog() override { return null_stream_; }
    std::ostream& DevLog() override { return null_stream_; }

    // Simulations resolve reactions immediately and never suspend.
    void HandleReactionTrigger(const TTriggerContext&, const TState&) override {}

protected:
    size_t ChooseAlternativeIndex(int player_id, const TAlternatives& alternatives) override
    {
        return strategy_.Decide(player_id, alternatives);
    }

private:
    class TNullBuffer : public std::streambuf {
    public:
        int overflow(int c) override { return c; }
    };

    const IDecisionStrategy& strategy_;
    TNullBuffer null_buffer_;
    std::ostream null_stream_;
};

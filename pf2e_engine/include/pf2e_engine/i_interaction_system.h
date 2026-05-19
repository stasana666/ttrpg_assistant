#pragma once

#include <pf2e_engine/alternatives.h>

#include <cassert>
#include <ostream>

struct TTriggerContext;
class TState;

class IInteractionSystem {
public:
    virtual ~IInteractionSystem() = default;

    template <class T>
    T ChooseAlternative(int player_id, const TAlternatives& alternatives)
    {
        assert(!alternatives.Empty());
        if (alternatives.Size() == 1) {
            return alternatives[0].Get<T>();
        }

        size_t index = ChooseAlternativeIndex(player_id, alternatives);
        return alternatives[index].Get<T>();
    }

    // Reports a reaction opportunity raised by the engine. The implementation
    // either resolves it immediately (returns normally) or defers it by throwing
    // TSavepointStackUnwind, suspending execution so it can be resumed later.
    // The engine never decides which happens -- it only reports the trigger.
    virtual void HandleReactionTrigger(const TTriggerContext& trigger, const TState& state) = 0;

    virtual std::ostream& GameLog() = 0;
    virtual std::ostream& DevLog() = 0;

protected:
    virtual size_t ChooseAlternativeIndex(int player_id, const TAlternatives& alternatives) = 0;
};

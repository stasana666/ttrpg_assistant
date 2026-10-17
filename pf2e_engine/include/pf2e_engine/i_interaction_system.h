#pragma once

#include <pf2e_engine/alternatives.h>

#include <cassert>
#include <functional>
#include <ostream>

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

    virtual void RequestChoice(int player_id, TAlternatives alternatives, TState state,
                               std::function<void(size_t)> receive);

    virtual std::ostream& GameLog() = 0;
    virtual std::ostream& DevLog() = 0;

protected:
    virtual size_t ChooseAlternativeIndex(int player_id, const TAlternatives& alternatives) = 0;
};

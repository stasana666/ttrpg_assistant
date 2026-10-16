#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/transformation/state.h>

#include <stdexcept>

void IInteractionSystem::RequestChoice(int player_id, TAlternatives alternatives,
                                      [[maybe_unused]] TState state,
                                      std::function<void(size_t)> receive)
{
    if (alternatives.Empty() || !receive) {
        throw std::invalid_argument("a choice requires alternatives and a receiver");
    }
    const auto index = alternatives.Size() == 1 ? 0 : ChooseAlternativeIndex(player_id, alternatives);
    if (index >= alternatives.Size()) {
        throw std::out_of_range("selected choice is outside the alternatives");
    }
    receive(index);
}

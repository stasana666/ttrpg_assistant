#pragma once

#include <pf2e_engine/alternatives.h>

#include <cassert>
#include <iostream>
#include <sstream>

class TInteractionSystem {
public:
    template <class T>
    T ChooseAlternative(int player_id, const TAlternatives<T>& alternatives);

    std::ostream& GameLog();
    std::ostream& DevLog();

private:

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

    std::stringstream ss;
    ss << "Ask " << player_id << "\n";
    ss << "Choose " << alternatives.Kind() << " and write it's number:" << std::endl;
    for (size_t i = 0; i < alternatives.Size(); ++i) {
        ss << i << " - " << alternatives[i].name << std::endl;
    }

    size_t result;
    do {
        std::cout << ss.str() << std::endl;
        std::cin >> result;
    } while (result >= alternatives.Size());

    return alternatives[result].value;
}

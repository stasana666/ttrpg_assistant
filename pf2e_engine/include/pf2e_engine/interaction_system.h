#pragma once

#include <cassert>
#include <functional>
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

class TInteractionSystem {
public:
    template <class T>
    T ChooseAlternative(int player_id, const std::vector<T>& alternatives,
        std::string_view header, std::function<std::string_view(const T&)> descriptor);

    std::ostream& GameLog();
    std::ostream& DevLog();

private:

};

template <class T>
T TInteractionSystem::ChooseAlternative(
    int player_id,
    const std::vector<T>& alternatives,
    std::string_view header,
    std::function<std::string_view(const T&)> descriptor)
{
    assert(!alternatives.empty());
    if (alternatives.size() == 1) {
        return alternatives[0];
    }

    std::stringstream ss;
    ss << "Ask " << player_id << "\n";
    ss << "Choose " << header << " and write it's number:" << std::endl;
    for (size_t i = 0; i < alternatives.size(); ++i) {
        ss << i << " - " << descriptor(alternatives[i]) << std::endl;
    }

    size_t result;
    do {
        std::cout << ss.str() << std::endl;
        std::cin >> result;
    } while (result >= alternatives.size());

    return alternatives[result];
}

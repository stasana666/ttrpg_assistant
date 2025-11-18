#pragma once

#include <SFML/Graphics.hpp>

#include <vector>

class TBoardGUI {
public:
    explicit TBoardGUI(size_t size);

    void Run();
private:
    void EventHandler();
    void BuildTiles();
    void Show();

    unsigned int size_;

    sf::RenderWindow window_;
    std::vector<sf::RectangleShape> tiles_;
};

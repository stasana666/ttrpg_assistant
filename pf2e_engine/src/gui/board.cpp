#include <board.h>

constexpr size_t kTileSize = 50;

TBoardGUI::TBoardGUI(size_t size)
    : size_(size)
    , window_(sf::VideoMode(sf::Vector2u(size_ * kTileSize, size * kTileSize)), "BattleMap")
{
    BuildTiles();
}

void TBoardGUI::Show()
{
    window_.clear(sf::Color::Black);
    for (const auto& r : tiles_) {
        window_.draw(r);
    }
    window_.display();
}

void TBoardGUI::Run()
{
    while (window_.isOpen()) {
        EventHandler();
        Show();
    }
}

void TBoardGUI::BuildTiles()
{
    tiles_.clear();
    tiles_.reserve(size_ * size_);
    for (unsigned int y = 0; y < size_; ++y) {
        for (unsigned int x = 0; x < size_; ++x) {
            sf::RectangleShape rect(
                sf::Vector2f(static_cast<float>(kTileSize), static_cast<float>(kTileSize)));
            rect.setPosition(sf::Vector2f(static_cast<float>(x * kTileSize), static_cast<float>(y * kTileSize)));
            rect.setFillColor(sf::Color(0, 200, 0));
            //rect.setOutlineThickness(1.f);
            //rect.setOutlineColor(sf::Color(50, 50, 50));
            tiles_.push_back(rect);
        }
    }
}

void TBoardGUI::EventHandler()
{
    while (const std::optional event = window_.pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            window_.close();
        }
        else if (const auto* key_pressed = event->getIf<sf::Event::KeyPressed>())
        {
            if (key_pressed->scancode == sf::Keyboard::Scancode::Escape) {
                window_.close();
            }
        }
    }
}

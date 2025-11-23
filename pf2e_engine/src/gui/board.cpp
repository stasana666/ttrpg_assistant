#include <board.h>

#include <pf2e_engine/battle_map.h>

#include <SFML/Graphics/Texture.hpp>

constexpr size_t kTileSize = 200;

TBoardGUI::TBoardGUI(THolder<TBattleMap>& battle_map, std::filesystem::path path_to_image_dir, TChannel<TClickEvent>::TProducer event_queue)
    : battle_map_(battle_map)
    , current_(battle_map_.Get())
    , window_(sf::VideoMode(sf::Vector2u(current_->GetXSize() * kTileSize, current_->GetYSize() * kTileSize)), "BattleMap")
    , textures_(path_to_image_dir)
    , event_queue_(event_queue)
{
    BuildTiles();
}

void TBoardGUI::Show()
{
    textures_.CommitFrame();
    current_ = battle_map_.Get();
    window_.clear(sf::Color::Black);
    BuildTiles();

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
    tiles_.reserve(current_->GetXSize() * current_->GetYSize());
    for (int y = 0; y < current_->GetXSize(); ++y) {
        for (int x = 0; x < current_->GetYSize(); ++x) {
            tiles_.push_back(BuildTile(x, y));
        }
    }
}

sf::RectangleShape TBoardGUI::BuildTile(size_t x, size_t y)
{
    auto cell = current_->GetCell(x, y);

    sf::RectangleShape rect(
        sf::Vector2f(static_cast<float>(kTileSize), static_cast<float>(kTileSize)));
    rect.setPosition(sf::Vector2f(static_cast<float>(x * kTileSize), static_cast<float>(y * kTileSize)));

    if (cell.player != nullptr) {
        sf::Texture& texture = textures_.Get(cell.player->GetImagePath());
        rect.setTexture(&texture);
    } else {
        rect.setFillColor(sf::Color(0, 200, 0));
    }

    return rect;
}

void TBoardGUI::EventHandler()
{
    while (const std::optional event = window_.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window_.close();
        } else if (const auto* key_pressed = event->getIf<sf::Event::KeyPressed>()) {
            if (key_pressed->scancode == sf::Keyboard::Scancode::Escape) {
                window_.close();
            }
        } else if (const sf::Event::MouseButtonPressed* mouse_pressed = event->getIf<sf::Event::MouseButtonPressed>()) {
            OnMousePressed(mouse_pressed);
        }
    }
}

void TBoardGUI::OnMousePressed(const sf::Event::MouseButtonPressed* mouse_pressed)
{
    const float screen_x = window_.getSize().x;
    const float screen_y = window_.getSize().y;

    const float tile_x = screen_x / current_->GetXSize();
    const float tile_y = screen_y / current_->GetYSize();

    event_queue_.Enqueue(TClickEvent{
        .value = TPosition{
            .x = static_cast<int>(mouse_pressed->position.x / tile_x),
            .y = static_cast<int>(mouse_pressed->position.y / tile_y)
        },
        .timepoint = std::chrono::steady_clock::now()
    });
}

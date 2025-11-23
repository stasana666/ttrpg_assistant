#pragma once

#include <SFML/Graphics.hpp>

#include <pf2e_engine/common/holder.h>
#include <pf2e_engine/gui/click_event.h>
#include <pf2e_engine/gui/texture_storage.h>
#include <pf2e_engine/common/channel.h>

#include <vector>

class TBattleMap;

class TBoardGUI {
public:
    TBoardGUI(THolder<TBattleMap>& battle_map, std::filesystem::path path_to_image_dir, TChannel<TClickEvent>::TProducer event_queue);

    void Run();
private:
    void EventHandler();
    void BuildTiles();
    sf::RectangleShape BuildTile(size_t x, size_t y);
    void Show();

    void OnMousePressed(const sf::Event::MouseButtonPressed* mouse_pressed);

    THolder<TBattleMap>& battle_map_;
    std::shared_ptr<const TBattleMap> current_;
    sf::RenderWindow window_;
    TTextureStorage textures_;
    std::vector<sf::RectangleShape> tiles_;
    TChannel<TClickEvent>::TProducer event_queue_;
};

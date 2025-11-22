#pragma once

#include <SFML/Graphics.hpp>

#include <pf2e_engine/common/holder.h>

#include <vector>
#include "pf2e_engine/gui/texture_storage.h"

class TBattleMap;

class TBoardGUI {
public:
    TBoardGUI(THolder<TBattleMap>& battle_map, std::filesystem::path path_to_image_dir);

    void Run();
private:
    void EventHandler();
    void BuildTiles();
    sf::RectangleShape BuildTile(size_t x, size_t y);
    void Show();

    THolder<TBattleMap>& battle_map_;
    std::shared_ptr<const TBattleMap> current_;
    sf::RenderWindow window_;
    TTextureStorage textures_;
    std::vector<sf::RectangleShape> tiles_;
};

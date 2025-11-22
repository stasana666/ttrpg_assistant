#pragma once

#include <SFML/Graphics.hpp>

#include <map>

class TTextureStorage {
public:
    explicit TTextureStorage(std::filesystem::path path_to_image_dir);

    sf::Texture& Get(const std::filesystem::path& path);
    void CommitFrame();

private:
    struct TTextureEntry {
        sf::Texture texture;
        int last_frame_use{};
    };

    std::filesystem::path path_to_image_dir_;
    std::map<std::filesystem::path, TTextureEntry> textures_;
    int frame_{};
};

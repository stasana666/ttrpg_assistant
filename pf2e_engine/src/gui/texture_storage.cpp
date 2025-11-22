#include <texture_storage.h>

TTextureStorage::TTextureStorage(std::filesystem::path path_to_image_dir)
    : path_to_image_dir_(std::move(path_to_image_dir))
{
}

sf::Texture& TTextureStorage::Get(const std::filesystem::path& path) {
    auto it = textures_.find(path);
    if (it != textures_.end()) {
        it->second.last_frame_use = frame_;
        return it->second.texture;
    }

    TTextureEntry entry;
    if (!entry.texture.loadFromFile(path_to_image_dir_ / path)) {
        throw std::runtime_error("Failed to load texture: " + (path_to_image_dir_ / path).string());
    }
    entry.last_frame_use = frame_;
    auto [new_it, inserted] = textures_.emplace(path, std::move(entry));
    assert(inserted);
    return new_it->second.texture;
}

void TTextureStorage::CommitFrame() {
    ++frame_;
    for (auto it = textures_.begin(); it != textures_.end();) {
        if (it->second.last_frame_use < frame_ - 1) {
            it = textures_.erase(it);
        } else {
            ++it;
        }
    }
}

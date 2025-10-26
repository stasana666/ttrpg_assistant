#include <pf2e_engine/battle_map.h>

#include <nlohmann/json.hpp>

TBattleMap::TBattleMap(nlohmann::json& json)
{
    x_size_ = json["x_size"];
    y_size_ = json["y_size"];

    battlemap_.resize(y_size_, std::vector<Cell>(x_size_, { nullptr }));
}

bool TBattleMap::HasLine(TPosition, TPosition) const
{
    return true;
}

bool TBattleMap::HasLine(TPosition src, TPosition dst, size_t max_length) const
{
    return (src.x - dst.x) * (src.x - dst.x) + (src.y - dst.y) * (src.y - dst.y) <= max_length * max_length
        && HasLine(src, dst);
}

void TBattleMap::AddPlayer(TPlayer* player)
{
    auto& cell = battlemap_[player->position.x][player->position.y];
    if (cell.player != nullptr) {
        throw std::runtime_error("cell already has another player");
    }
    cell.player = player;
}

TPosition TBattleMap::ChoosePosition() const
{
    for (size_t x = 0; x < x_size_; ++x) {
        for (size_t y = 0; y < y_size_; ++y) {
            if (battlemap_[x][y].player == nullptr) {
                return TPosition{x, y};
            }
        }
    }
    throw std::runtime_error("too many creatures for battle map");
}

#include <pf2e_engine/battle_map.h>

#include <nlohmann/json.hpp>

TBattleMap::TBattleMap(nlohmann::json& json)
{
    x_size_ = json["x_size"];
    y_size_ = json["y_size"];

    battlemap_.resize(y_size_, std::vector<Cell>(x_size_, { nullptr }));
}

bool TBattleMap::HasLine(TPosition src, TPosition dst) const
{
    return true;
}

bool TBattleMap::HasLine(TPosition src, TPosition dst, size_t max_length) const
{
    return (src.x - dst.x) * (src.x - dst.x) + (src.y - dst.y) * (src.y - dst.y) <= max_length * max_length
        && HasLine(src, dst);
}

std::vector<TPlayer*> TBattleMap::GetIfPlayers(std::function<bool(const TPlayer*)> predicate)
{
    std::vector<TPlayer*> result;
    for (auto& player : players_) {
        if (predicate(&player)) {
            result.emplace_back(&player);
        }
    }
    return result;
}

const TPlayer* TBattleMap::GetPlayer(std::function<bool(const TPlayer*)> predicate) const
{
    for (auto& player : players_) {
        if (predicate(&player)) {
            return &player;
        }
    }
    return nullptr;
}

TPosition TBattleMap::GetPosition(TCreature* creature) const
{
    const TPlayer* player = GetPlayer([creature](const TPlayer* player) {
        return player->creature == creature;
    });
    if (player == nullptr) {
        throw std::logic_error("player by creature not found");
    }
    return player->position;
}

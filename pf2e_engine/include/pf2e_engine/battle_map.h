#pragma once

#include <pf2e_engine/creature.h>

#include <limits>
#include <nlohmann/json_fwd.hpp>

#include <deque>

struct TPosition {
    size_t x;
    size_t y;
};

struct TPlayer {
    int team;
    TPosition position;
    TCreature* creature;
};

class TBattleMap {
public:
    explicit TBattleMap(nlohmann::json& json);

    struct Cell {
        TPlayer* player;
    };

    bool HasLine(TPosition src, TPosition dst) const;
    bool HasLine(TPosition src, TPosition dst, size_t max_length) const;

    const TPlayer* GetPlayer(std::function<bool(const TPlayer*)> predicate) const;
    std::vector<TPlayer*> GetIfPlayers(std::function<bool(const TPlayer*)> predicate);

    TPosition GetPosition(TCreature* creature) const;

private:
    size_t x_size_;
    size_t y_size_;
    std::vector<std::vector<Cell>> battlemap_;
    std::deque<TPlayer> players_;
};

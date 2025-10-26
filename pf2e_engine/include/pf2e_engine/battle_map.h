#pragma once

#include <pf2e_engine/player.h>

#include <nlohmann/json_fwd.hpp>

class TBattleMap {
public:
    explicit TBattleMap(nlohmann::json& json);

    struct Cell {
        TPlayer* player;
    };

    bool HasLine(TPosition src, TPosition dst) const;
    bool HasLine(TPosition src, TPosition dst, size_t max_length) const;

    void AddPlayer(TPlayer* player);

    TPosition GetPosition(TCreature* creature) const;

private:
    TPosition ChoosePosition() const;

    size_t x_size_;
    size_t y_size_;
    std::vector<std::vector<Cell>> battlemap_;
};

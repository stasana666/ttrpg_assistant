#pragma once

#include <pf2e_engine/player.h>

#include <nlohmann/json_fwd.hpp>

class TBattleMap {
public:
    explicit TBattleMap(nlohmann::json& json);

    struct TCell {
        TPlayer* player;
    };

    int GetXSize() const;
    int GetYSize() const;

    bool HasLine(TPosition src, TPosition dst) const;
    bool HasLine(TPosition src, TPosition dst, int max_length) const;

    // Area geometry checks
    bool InRadius(TPosition center, int radius, TPosition target) const;
    bool InCone(TPosition apex, TPosition direction_cell, int length, TPosition target) const;
    bool InLine(TPosition start, TPosition direction_cell, int length, int width, TPosition target) const;

    const TCell& GetCell(int x, int y) const;
    TCell& GetCell(int x, int y);

private:
    TPosition ChoosePosition() const;

    int x_size_;
    int y_size_;
    std::vector<std::vector<TCell>> battlemap_;
};

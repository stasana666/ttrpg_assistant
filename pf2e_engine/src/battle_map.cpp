#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/vector2d.h>

#include <nlohmann/json.hpp>

TBattleMap::TBattleMap(nlohmann::json& json)
{
    x_size_ = json["x_size"];
    y_size_ = json["y_size"];

    battlemap_.resize(y_size_, std::vector<TCell>(x_size_, { nullptr }));
}

int TBattleMap::GetXSize() const
{
    return x_size_;
}

int TBattleMap::GetYSize() const
{
    return y_size_;
}

bool TBattleMap::HasLine(TPosition, TPosition) const
{
    return true;
}

bool TBattleMap::HasLine(TPosition src, TPosition dst, int max_length) const
{
    return (src.x - dst.x) * (src.x - dst.x) * 4 + (src.y - dst.y) * (src.y - dst.y) * 4 <= (2 * max_length + 1) * (2 * max_length + 1)
        && HasLine(src, dst);
}

bool TBattleMap::InRadius(TPosition center, int radius, TPosition target) const
{
    return HasLine(center, target, radius);
}

bool TBattleMap::InCone(TPosition apex, TPosition direction_cell, int length, TPosition target) const
{
    TVector2D dir = TVector2D(direction_cell) - TVector2D(apex);
    if (dir.IsZero()) {
        throw std::logic_error("InCone: direction vector is zero");
    }
    dir = dir.Normalized();

    TVector2D to_target = TVector2D(target) - TVector2D(apex);
    if (to_target.IsZero()) {
        return false;
    }

    if (to_target.Length() > length) {
        return false;
    }

    double dot = dir.Dot(to_target.Normalized());
    return dot >= TVector2D::kConeHalfAngleCos;
}

bool TBattleMap::InLine(TPosition start, TPosition direction_cell, int length, int width, TPosition target) const
{
    TVector2D dir = TVector2D(direction_cell) - TVector2D(start);
    if (dir.IsZero()) {
        throw std::logic_error("InLine: direction vector is zero");
    }
    dir = dir.Normalized();

    TVector2D to_target = TVector2D(target) - TVector2D(start);

    double along = dir.Dot(to_target);
    if (along < 0 || along > length) {
        return false;
    }

    double perp = std::abs(dir.Cross(to_target));
    return perp <= static_cast<double>(width) / 2.0;
}

TPosition TBattleMap::ChoosePosition() const
{
    for (int x = 0; x < x_size_; ++x) {
        for (int y = 0; y < y_size_; ++y) {
            if (battlemap_[x][y].player == nullptr) {
                return TPosition{x, y};
            }
        }
    }
    throw std::runtime_error("too many creatures for battle map");
}

const TBattleMap::TCell& TBattleMap::GetCell(int x, int y) const
{
    return battlemap_[x][y];
}

TBattleMap::TCell& TBattleMap::GetCell(int x, int y)
{
    return battlemap_[x][y];
}

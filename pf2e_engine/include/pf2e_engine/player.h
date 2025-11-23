#pragma once

#include <pf2e_engine/creature.h>
#include <pf2e_engine/common/holder.h>
#include <pf2e_engine/position.h>

#include <filesystem>

struct TPlayerId {
    int id;
};

struct TPlayerTeam {
    int team;
};

class TBattleMap;

class TPlayer {
public:
    TPlayer(TCreature* creature, TPlayerTeam team, TPlayerId id,
        std::string name, std::filesystem::path image_path);

    const TCreature* GetCreature() const;
    TCreature* GetCreature();
    int GetId() const;
    int GetTeam() const;
    TPosition GetPosition() const;
    void SetPosition(TPosition new_position);
    std::string_view GetName() const;
    const std::filesystem::path& GetImagePath() const;

    void BindWith(THolder<TBattleMap>& battle_map, const TPosition position);
    void Unbind();

private:
    TCreature* creature_;
    TPlayerTeam team_;
    TPlayerId id_;
    TPosition position_;
    std::string name_;
    std::filesystem::path image_path_;
    THolder<TBattleMap>* battle_map_;
};

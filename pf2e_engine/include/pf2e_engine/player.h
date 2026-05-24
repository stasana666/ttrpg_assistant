#pragma once

#include <pf2e_engine/creature.h>
#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/holder.h>
#include <pf2e_engine/position.h>

#include <filesystem>
#include <string>

struct TPlayerId {
    int id;
};

inline std::string AstSerialize(const TPlayerId& v)
{
    return "TPlayerId{" + std::to_string(v.id) + "}";
}

struct TPlayerTeam {
    int team;
};

inline std::string AstSerialize(const TPlayerTeam& v)
{
    return "TPlayerTeam{" + std::to_string(v.team) + "}";
}

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

    // creature_ is a non-owning raw pointer in this codebase (TCreature is
    // owned outside the battle — typically by a test fixture or app). For AST
    // purposes we *do* recurse into it: creature state (HP, conditions,
    // resources) is the bulk of what save/rollback must restore, and in
    // practice there is exactly one TPlayer per TCreature. The cycle set
    // catches any violation of that 1:1 assumption.
    TAstNode GetAst(TAstContext& ctx) const;

private:
    TCreature* creature_;
    TPlayerTeam team_;
    TPlayerId id_;
    TPosition position_;
    std::string name_;
    std::filesystem::path image_path_;
    THolder<TBattleMap>* battle_map_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TPlayer> : std::true_type {};

#include <player.h>

#include <pf2e_engine/battle_map.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

#include <stdexcept>

using TCell = TBattleMap::TCell;

TPlayer::TPlayer(TCreature* creature, TPlayerTeam team, TPlayerId id,
    std::string name, std::filesystem::path image_path)
    : creature_(creature)
    , team_(team)
    , id_(id)
    , name_(name)
    , image_path_(image_path)
    , battle_map_(nullptr)
{
}

const TCreature* TPlayer::GetCreature() const
{
    return creature_;
}

TCreature* TPlayer::GetCreature()
{
    return creature_;
}

int TPlayer::GetId() const
{
    return id_.id;
}

int TPlayer::GetTeam() const
{
    return team_.team;
}

TPosition TPlayer::GetPosition() const
{
    return position_;
}

void TPlayer::SetPosition(TPosition new_position)
{
    assert(battle_map_ != nullptr);
    auto copy = battle_map_->Copy();
    TCell& old_cell = copy->GetCell(position_.x, position_.y);
    position_ = new_position;
    TCell& new_cell = copy->GetCell(position_.x, position_.y);
    assert(new_cell.player == nullptr);
    new_cell.player = old_cell.player;
    old_cell.player = nullptr;
    battle_map_->Set(copy);
}

std::string_view TPlayer::GetName() const
{
    return name_;
}

const std::filesystem::path& TPlayer::GetImagePath() const
{
    return image_path_;
}

void TPlayer::BindWith(THolder<TBattleMap>& battle_map, const TPosition position)
{
    assert(battle_map_ == nullptr);
    auto copy = battle_map.Copy();
    TBattleMap::TCell& cell = copy->GetCell(position.x, position.y);
    if (cell.player != nullptr) {
        throw std::runtime_error("two creature in one cell is not supported by rules");
    }
    cell.player = this;
    position_ = position;
    battle_map_ = &battle_map;
    battle_map_->Set(copy);
}

void TPlayer::Unbind()
{
    battle_map_ = nullptr;
}

TAstNode TPlayer::GetAst(TAstContext& ctx) const
{
    static constexpr size_t kExpectedSize = 112;
    AST_ASSERT_LAYOUT(TPlayer, kExpectedSize);

    const std::string my_id = "player#" + std::to_string(GetId());
    ctx.RegisterIdentity(this, my_id);
    if (creature_ != nullptr) {
        ctx.RegisterIdentity(creature_, my_id + ".creature");
    }

    TAstNode node = TAstNode::MakeObject("TPlayer");
    AddValueField(node, "id", id_);
    AddValueField(node, "team", team_);
    AddValueField(node, "name", name_);
    AddValueField(node, "image_path", image_path_);
    AddValueField(node, "position", position_);
    AddReference(node, "battle_map", battle_map_, "battle.map_holder");
    AddOwnedObject(node, "creature", creature_, ctx);
    return node;
}

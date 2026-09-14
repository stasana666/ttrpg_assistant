#include <player.h>

#include <pf2e_engine/battle_map.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

#include <stdexcept>

TPlayer::TPlayer(TCreature* creature, TPlayerTeam team, TPlayerId id,
    std::string name, std::filesystem::path image_path)
    : creature_(creature)
    , team_(team)
    , id_(id)
    , position_{}
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
    if (battle_map_ == nullptr) {
        throw std::logic_error("movement requires a bound player");
    }
    const auto snapshot = battle_map_->Get();
    if (!snapshot->Contains(new_position)) {
        throw std::out_of_range("player destination is outside the map");
    }
    if (!snapshot->Contains(position_)
        || snapshot->GetCell(position_.x, position_.y).player != this) {
        throw std::logic_error("player position does not match its occupied cell");
    }
    if (new_position == position_) {
        return;
    }
    if (snapshot->GetCell(new_position.x, new_position.y).player != nullptr) {
        throw std::runtime_error("two creatures in one cell is not supported by rules");
    }
    auto copy = std::make_shared<TBattleMap>(*snapshot);
    copy->GetCell(new_position.x, new_position.y).player = this;
    copy->GetCell(position_.x, position_.y).player = nullptr;
    battle_map_->Set(std::move(copy));
    position_ = new_position;
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
    if (battle_map_ != nullptr) {
        throw std::logic_error("player is already bound to a battle map");
    }
    const auto snapshot = battle_map.Get();
    if (!snapshot->Contains(position)) {
        throw std::out_of_range("player position is outside the map");
    }
    if (snapshot->GetCell(position.x, position.y).player != nullptr) {
        throw std::runtime_error("two creatures in one cell is not supported by rules");
    }
    auto copy = std::make_shared<TBattleMap>(*snapshot);
    copy->GetCell(position.x, position.y).player = this;
    battle_map.Set(std::move(copy));
    position_ = position;
    battle_map_ = &battle_map;
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

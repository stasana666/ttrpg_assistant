#include <weapon_slot.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

auto TWeaponSlots::Equip(THoldedWeapon weapon) -> TWeaponDescriptor
{
    weapons_.emplace_back(weapon);
    return TWeaponDescriptor(this, weapons_.size() - 1);
}

auto TWeaponSlots::operator [](size_t idx) -> TWeaponDescriptor
{
    return TWeaponDescriptor(this, idx);
}

const TWeapon& TWeaponSlots::WeaponAt(size_t idx) const
{
    return weapons_[idx].weapon;
}

size_t TWeaponSlots::Size() const
{
    return weapons_.size();
}

bool TWeaponSlots::Empty() const
{
    return weapons_.empty();
}

TWeaponSlots::TWeaponDescriptor::TWeaponDescriptor(TWeaponSlots* parent, size_t index)
    : parent_(parent)
    , index_(index)
{
}

void TWeaponSlots::TWeaponDescriptor::SetGrip(size_t hand_count)
{
    parent_->weapons_[index_].hand_count = hand_count;
}

int TWeaponSlots::TWeaponDescriptor::Grip() const
{
    return parent_->weapons_[index_].hand_count;
}

TWeapon& TWeaponSlots::TWeaponDescriptor::Weapon()
{
    return parent_->weapons_[index_].weapon;
}

const TWeapon& TWeaponSlots::TWeaponDescriptor::Weapon() const
{
    return parent_->weapons_[index_].weapon;
}

TAstNode TWeaponSlots::THoldedWeapon::GetAst(TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("THoldedWeapon");
    AddOwnedObject(node, "weapon", weapon, ctx);
    AddValueField(node, "hand_count", hand_count);
    return node;
}

TAstNode TWeaponSlots::GetAst(TAstContext& ctx) const
{
    static constexpr size_t kExpectedSize = 32;
    static constexpr size_t kExpectedSentinelOffset = 24;
    AST_ASSERT_LAYOUT_WITH_SENTINEL(TWeaponSlots, kExpectedSize, kExpectedSentinelOffset);

    TAstNode node = TAstNode::MakeObject("TWeaponSlots");
    TAstNode slots = TAstNode::MakeObject("slots");
    for (size_t i = 0; i < weapons_.size(); ++i) {
        AddOwnedObject(slots, std::to_string(i), weapons_[i], ctx);
    }
    node.AddChild("weapons", std::move(slots));
    return node;
}

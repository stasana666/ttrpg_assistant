#include <pf2e_engine/creature.h>

#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/common/errors.h>
#include <pf2e_engine/proficiency.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

#include <algorithm>

TCreature::TCreature(TCharacteristicSet stats, TProficiency proficiency, TArmor armor, THitPoints hitpoints)
    : stats_(stats)
    , proficiency_(proficiency)
    , hitpoints_(hitpoints)
    , armor_(armor)
{
}

const TCharacteristic& TCreature::GetCharacteristic(ECharacteristic name) const
{
    return stats_[name];
}

int TCreature::GetLevel() const
{
    return proficiency_.GetLevel();
}

TCharacteristicSet& TCreature::Characteristics()
{
    return stats_;
}

THitPoints& TCreature::Hitpoints()
{
    return hitpoints_;
}

const THitPoints& TCreature::Hitpoints() const
{
    return hitpoints_;
}

const TResourcePool& TCreature::Resources() const
{
    return resources_;
}

TResourcePool& TCreature::Resources()
{
    return resources_;
}

const TArmor& TCreature::Armor() const
{
    return armor_;
}

TWeaponSlots& TCreature::Weapons()
{
    return weapons_;
}

std::vector<TWeapon>& TCreature::NaturalWeapons()
{
    return natural_weapons_;
}

const std::vector<TWeapon>& TCreature::NaturalWeapons() const
{
    return natural_weapons_;
}

int TCreature::MaxWeaponReach() const
{
    int reach = 0;
    auto consider = [&](const TWeapon& w) {
        reach = std::max(reach, w.Reach());
    };
    for (size_t i = 0; i < weapons_.Size(); ++i) {
        consider(weapons_.WeaponAt(i));
    }
    for (const TWeapon& weapon : natural_weapons_) {
        consider(weapon);
    }
    return reach;
}

const TProficiency& TCreature::Proficiency() const
{
    return proficiency_;
}

TProficiency& TCreature::Proficiency()
{
    return proficiency_;
}

const TDamageResolver& TCreature::DamageResolver() const
{
    return resolver_;
}

bool TCreature::IsAlive() const
{
    return hitpoints_.GetCurrentHp() > 0;
}

void TCreature::AddAction(std::shared_ptr<TAction> action)
{
    actions_.emplace_back(action);
}

std::vector<std::shared_ptr<TAction>>& TCreature::Actions()
{
    return actions_;
}

void TCreature::AddFeat(std::shared_ptr<TCreatureFeat> feat)
{
    feats_.emplace_back(std::move(feat));
}

const std::vector<std::shared_ptr<TCreatureFeat>>& TCreature::Feats() const
{
    return feats_;
}

int TCreature::Get(ECondition condition) const
{
    auto it = conditions_.find(condition);
    if (it == conditions_.end()) {
        return 0;
    }
    return it->second;
}

void TCreature::Set(ECondition condition, int value)
{
    conditions_[condition] = value;
}

int& TCreature::Movement()
{
    return movement_;
}

ECreatureSize TCreature::Size() const
{
    return size_;
}

void TCreature::SetSize(ECreatureSize size)
{
    size_ = size;
}

std::vector<const TReaction*> TCreature::Reactions(ETrigger trigger_type) const
{
    std::vector<const TReaction*> reactions;
    for (auto& reaction : reactions_) {
        if (reaction->TriggerType() == trigger_type) {
            reactions.emplace_back(reaction.get());
        }
    }
    return reactions;
}

TAstNode GetActionListAst(const std::vector<std::shared_ptr<TAction>>& actions)
{
    TAstNode node = TAstNode::MakeObject("actions");
    AddValueField(node, "count", actions.size());
    for (size_t i = 0; i < actions.size(); ++i) {
        AddValueField(node, std::to_string(i),
            actions[i] ? std::string(actions[i]->Name()) : std::string("<null>"));
    }
    return node;
}

TAstNode GetReactionListAst(const std::vector<std::shared_ptr<TReaction>>& reactions)
{
    TAstNode node = TAstNode::MakeObject("reactions");
    AddValueField(node, "count", reactions.size());
    for (size_t i = 0; i < reactions.size(); ++i) {
        if (reactions[i]) {
            AddValueField(node, std::to_string(i), reactions[i]->TriggerType());
        } else {
            AddValueField(node, std::to_string(i), std::string("<null>"));
        }
    }
    return node;
}

TAstNode TCreature::GetAst(TAstContext& ctx) const
{
    static constexpr size_t kExpectedSize = 888;
    AST_ASSERT_LAYOUT(TCreature, kExpectedSize);

    const std::string my_id = ctx.IdentityOf(this);
    if (!my_id.empty()) {
        ctx.RegisterIdentity(&hitpoints_, my_id + ".hitpoints");
        ctx.RegisterIdentity(&resources_, my_id + ".resources");
    }

    TAstNode node = TAstNode::MakeObject("TCreature");
    AddOwnedObject(node, "stats", stats_, ctx);
    AddOwnedObject(node, "proficiency", proficiency_, ctx);
    node.AddChild("conditions", GetConditionsAst(conditions_));
    AddOwnedObject(node, "hitpoints", hitpoints_, ctx);
    AddOwnedObject(node, "resolver", resolver_, ctx);
    AddOwnedObject(node, "resources", resources_, ctx);
    AddValueField(node, "movement", movement_);
    AddValueField(node, "size", size_);
    AddOwnedObject(node, "armor", armor_, ctx);
    AddOwnedObject(node, "weapons", weapons_, ctx);

    TAstNode natural = TAstNode::MakeObject("natural_weapons");
    for (size_t i = 0; i < natural_weapons_.size(); ++i) {
        AddOwnedObject(natural, std::to_string(i), natural_weapons_[i], ctx);
    }
    node.AddChild("natural_weapons", std::move(natural));

    node.AddChild("actions", GetActionListAst(actions_));
    node.AddChild("reactions", GetReactionListAst(reactions_));

    TAstNode feats_node = TAstNode::MakeObject("feats");
    for (size_t i = 0; i < feats_.size(); ++i) {
        if (feats_[i]) {
            AddOwnedObject(feats_node, std::to_string(i), *feats_[i], ctx);
        } else {
            feats_node.AddChild(std::to_string(i), TAstNode::MakeNull());
        }
    }
    node.AddChild("feats", std::move(feats_node));

    return node;
}

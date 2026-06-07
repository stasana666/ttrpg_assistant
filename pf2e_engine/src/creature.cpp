#include <pf2e_engine/creature.h>

#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/common/errors.h>
#include <pf2e_engine/proficiency.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

#include <algorithm>

TCreature::TCreature(TCreatureData data, TProficiency proficiency)
    : TCreatureData(std::move(data))
    , proficiency_(proficiency)
{
}

TAbilityScore TCreature::GetCharacteristic(ECharacteristic name) const
{
    const TAbilityScores& scores = Characteristic();
    switch (name) {
        case ECharacteristic::Strength:     return scores.Strength();
        case ECharacteristic::Dexterity:    return scores.Dexterity();
        case ECharacteristic::Constitution: return scores.Constitution();
        case ECharacteristic::Intelligence: return scores.Intelligence();
        case ECharacteristic::Wisdom:       return scores.Wisdom();
        case ECharacteristic::Charisma:     return scores.Charisma();
    }
    throw std::logic_error("unreachable");
}

int TCreature::GetLevel() const
{
    return proficiency_.GetLevel();
}

TGuarded<TBoundedQuantity> TCreature::Hitpoints()
{
    return TCreatureData::Hitpoints();
}

const TBoundedQuantity& TCreature::Hitpoints() const
{
    return TCreatureData::Hitpoints();
}

const TResourcePool& TCreature::Resources() const
{
    return resources_;
}

TGuarded<TResourcePool> TCreature::Resources()
{
    return TGuarded<TResourcePool>(resources_);
}

int TCreature::MaxWeaponReach() const
{
    int reach = 0;
    for (auto weapon : Weapons()) {
        reach = std::max(reach, weapon->Reach());
    }
    for (auto weapon : NaturalWeapons()) {
        reach = std::max(reach, weapon->Reach());
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

bool TCreature::IsAlive() const
{
    return Hitpoints().CurrentValue() > 0;
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

int TCreature::Get(EConditionKind condition) const
{
    if (!Conditions().Has(condition)) {
        return 0;
    }
    switch (condition) {
        case EConditionKind::Prone:
            return 1;
        case EConditionKind::Frightened:
            return Conditions().Get<TConditionFrightened>()->Value;
        case EConditionKind::MultipleAttackPenalty:
            return Conditions().Get<TConditionMultipleAttackPenalty>()->Value;
    }
    throw std::logic_error("unreachable");
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
    static constexpr size_t kExpectedSize = 792;
    AST_ASSERT_LAYOUT(TCreature, kExpectedSize);

    const std::string my_id = ctx.IdentityOf(this);
    if (!my_id.empty()) {
        ctx.RegisterIdentity(&Hitpoints(), my_id + ".hitpoints");
        ctx.RegisterIdentity(&resources_, my_id + ".resources");
    }

    TAstNode node = TAstNode::MakeObject("TCreature");
    node.AddChild("creature_data", TCreatureData::GetAst(ctx));
    AddOwnedObject(node, "proficiency", proficiency_, ctx);
    AddOwnedObject(node, "resources", resources_, ctx);

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

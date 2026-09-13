#include <pf2e_engine/creature.h>

#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/common/errors.h>
#include <pf2e_engine/proficiency.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_layout_assert.h>

#include <algorithm>

TCreature::TCreature(TCreatureData data,
                     std::shared_ptr<const TAbilityRules> abilities)
    : TCreatureData(std::move(data))
    , abilities_(abilities ? std::move(abilities) : std::make_shared<const TAbilityRules>())
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
    return Level();
}

TGuarded<THitPoints> TCreature::Hitpoints()
{
    return TCreatureData::Hitpoints();
}

const THitPoints& TCreature::Hitpoints() const
{
    return TCreatureData::Hitpoints();
}

const TResource& TCreature::ResourceFor(EResourceKind kind) const
{
    switch (kind) {
        case EResourceKind::Action:   return TCreatureData::Actions();
        case EResourceKind::Reaction: return TCreatureData::Reactions();
    }
    throw std::logic_error("unreachable");
}

TGuarded<TResource> TCreature::ResourceFor(EResourceKind kind)
{
    switch (kind) {
        case EResourceKind::Action:   return TCreatureData::Actions();
        case EResourceKind::Reaction: return TCreatureData::Reactions();
    }
    throw std::logic_error("unreachable");
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

bool TCreature::IsAlive() const
{
    return Hitpoints().Current() > 0;
}

const TAbilityRules& TCreature::AbilityRules() const
{
    return *abilities_;
}

TAbilityCollector TCreature::CollectAbilities() const
{
    TAbilityCollector collector(abilities_->Catalog);
    VisitGrantedAbilities(collector);
    return collector;
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

TAstNode TCreature::GetAst(TAstContext& ctx) const
{
    static constexpr size_t kExpectedSize = 960;
    AST_ASSERT_LAYOUT(TCreature, kExpectedSize);

    const std::string my_id = ctx.IdentityOf(this);
    if (!my_id.empty()) {
        ctx.RegisterIdentity(&Hitpoints(), my_id + ".hitpoints");
        ctx.RegisterIdentity(&TCreatureData::Actions(), my_id + ".actions");
        ctx.RegisterIdentity(&TCreatureData::Reactions(), my_id + ".reactions");
        ctx.RegisterIdentity(&Hands(), my_id + ".hands");
        for (const auto& [rank, slot] : SpellSlots()) {
            ctx.RegisterIdentity(&slot, my_id + ".spell_slots." + ToString(rank));
        }
    }

    TAstNode node = TAstNode::MakeObject("TCreature");
    node.AddChild("creature_data", TCreatureData::GetAst(ctx));

    TAstNode abilities = TAstNode::MakeObject("granted_abilities");
    const auto collector = CollectAbilities();
    size_t ability_index = 0;
    for (const auto& grant : collector.Grants()) {
        TAstNode binding = TAstNode::MakeObject("grant");
        AddValueField(binding, "definition", grant.DefinitionId);
        AddValueField(binding, "kind", static_cast<int>(grant.Kind));
        AddValueField(binding, "source", grant.Source.ToString());
        for (size_t i = 0; i < grant.Provenance.size(); ++i) {
            AddValueField(binding, "site_" + std::to_string(i), grant.Provenance[i]);
        }
        abilities.AddChild(std::to_string(ability_index++), std::move(binding));
    }
    node.AddChild("granted_abilities", std::move(abilities));


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

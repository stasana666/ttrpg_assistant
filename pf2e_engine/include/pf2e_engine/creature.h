#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/bounded_quantity.h>
#include <pf2e_engine/common/guarded.h>
#include <pf2e_engine/common/resource.h>

#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/actions/reaction.h>
#include <pf2e_engine/condition.h>
#include <pf2e_engine/feat.h>
#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/creature_data.h>
#include <pf2e_engine/inventory/creature_parts.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/proficiency.h>

class TCreature : public TCreatureData {
public:
    TCreature(TCreatureData data, TProficiency proficiency);

    TAbilityScore GetCharacteristic(ECharacteristic name) const;

    TGuarded<TBoundedQuantity> Hitpoints();
    const TBoundedQuantity& Hitpoints() const;

    const TResource& ResourceFor(EResourceKind kind) const;
    TGuarded<TResource> ResourceFor(EResourceKind kind);

    int MaxWeaponReach() const;

    int GetLevel() const;

    const TProficiency& Proficiency() const;
    TProficiency& Proficiency();

    bool IsAlive() const;
    void AddAction(std::shared_ptr<TAction> action);
    std::vector<std::shared_ptr<TAction>>& ActionList();

    void AddFeat(std::shared_ptr<TCreatureFeat> feat);
    const std::vector<std::shared_ptr<TCreatureFeat>>& Feats() const;

    int Get(EConditionKind condition) const;

    std::vector<const TReaction*> Reactions(ETrigger) const;

    TAstNode GetAst(TAstContext& ctx) const;

private:
    friend class TGameObjectFactory;

    TProficiency proficiency_;

    std::vector<std::shared_ptr<TAction>> actions_;
    std::vector<std::shared_ptr<TReaction>> reactions_;
    std::vector<std::shared_ptr<TCreatureFeat>> feats_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TCreature> : std::true_type {};

TAstNode GetActionListAst(const std::vector<std::shared_ptr<TAction>>& actions);
TAstNode GetReactionListAst(const std::vector<std::shared_ptr<TReaction>>& reactions);

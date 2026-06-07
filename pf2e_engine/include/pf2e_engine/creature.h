#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/guarded.h>

#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/actions/reaction.h>
#include <pf2e_engine/condition.h>
#include <pf2e_engine/feat.h>
#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/creature_data.h>
#include <pf2e_engine/inventory/creature_parts.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/mechanics/hitpoints.h>
#include <pf2e_engine/proficiency.h>

class TCreature : public TCreatureData {
public:
    TCreature(TCreatureData data, TProficiency proficiency, THitPoints hitpoints);

    TAbilityScore GetCharacteristic(ECharacteristic name) const;

    TGuarded<THitPoints> Hitpoints();
    const THitPoints& Hitpoints() const;

    const TResourcePool& Resources() const;
    TGuarded<TResourcePool> Resources();

    int MaxWeaponReach() const;

    int GetLevel() const;

    const TProficiency& Proficiency() const;
    TProficiency& Proficiency();

    bool IsAlive() const;
    void AddAction(std::shared_ptr<TAction> action);
    std::vector<std::shared_ptr<TAction>>& Actions();

    void AddFeat(std::shared_ptr<TCreatureFeat> feat);
    const std::vector<std::shared_ptr<TCreatureFeat>>& Feats() const;

    int Get(EConditionKind condition) const;

    std::vector<const TReaction*> Reactions(ETrigger) const;

    TAstNode GetAst(TAstContext& ctx) const;

private:
    friend class TGameObjectFactory;
    TResourcePool& ResourcesForInit() { return resources_; }

    TProficiency proficiency_;

    THitPoints hitpoints_;

    TResourcePool resources_;

    std::vector<std::shared_ptr<TAction>> actions_;
    std::vector<std::shared_ptr<TReaction>> reactions_;
    std::vector<std::shared_ptr<TCreatureFeat>> feats_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TCreature> : std::true_type {};

TAstNode GetActionListAst(const std::vector<std::shared_ptr<TAction>>& actions);
TAstNode GetReactionListAst(const std::vector<std::shared_ptr<TReaction>>& reactions);

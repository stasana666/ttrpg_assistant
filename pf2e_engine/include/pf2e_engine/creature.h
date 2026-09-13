#pragma once

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/guarded.h>
#include <pf2e_engine/common/resource.h>

#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/reaction.h>
#include <pf2e_engine/condition.h>
#include <pf2e_engine/feat.h>
#include <pf2e_engine/inventory/armor.h>
#include <pf2e_engine/inventory/creature_data.h>
#include <pf2e_engine/inventory/creature_parts.h>
#include <pf2e_engine/inventory/hitpoints.h>
#include <pf2e_engine/mechanics/characteristics.h>
#include <pf2e_engine/proficiency.h>

class TCreature : public TCreatureData {
public:
    TCreature(TCreatureData data, TProficiency proficiency,
              std::shared_ptr<const TAbilityRules> abilities = nullptr);

    TAbilityScore GetCharacteristic(ECharacteristic name) const;

    TGuarded<THitPoints> Hitpoints();
    const THitPoints& Hitpoints() const;

    const TResource& ResourceFor(EResourceKind kind) const;
    TGuarded<TResource> ResourceFor(EResourceKind kind);

    int MaxWeaponReach() const;

    int GetLevel() const;

    const TProficiency& Proficiency() const;
    TProficiency& Proficiency();

    bool IsAlive() const;
    const TAbilityRules& AbilityRules() const;
    TAbilityCollector CollectAbilities() const;

    void AddFeat(std::shared_ptr<TCreatureFeat> feat);
    const std::vector<std::shared_ptr<TCreatureFeat>>& Feats() const;

    int Get(EConditionKind condition) const;

    TAstNode GetAst(TAstContext& ctx) const;

private:
    friend class TGameObjectFactory;

    TProficiency proficiency_;

    std::shared_ptr<const TAbilityRules> abilities_;
    std::vector<std::shared_ptr<TCreatureFeat>> feats_;
    [[maybe_unused]] char ast_layout_sentinel_[1] = {};
};

template <>
struct TIsAstRecursive<TCreature> : std::true_type {};

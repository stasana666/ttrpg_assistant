// AUTO-GENERATED FROM computed.ttrpg -- DO NOT EDIT.
#include <computed.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/actions/ability.h>

#include <stdexcept>
#include <string>
#include <utility>

TPart::TPart() : TPart(Create({})) {}

TPart::TInitialValues TPart::ResolveInitialValues(const TInitialValues& initial) {
    TPart result(TUninitialized{}, initial.Bonus ? *initial.Bonus : TGeneratedConstruction::Blank<int>());
    [[maybe_unused]] auto& self = result;
    (void)initial;
    result.Bonus_ = initial.Bonus ? *initial.Bonus : int{};
    return TInitialValues{.Bonus = std::move(result.Bonus_), };
}

TPart TPart::Create(const TInitialValues& initial) {
    [[maybe_unused]] auto values = ResolveInitialValues(initial);
    TPart result(TUninitialized{}, std::move(*values.Bonus));
    return result;
}

TAstNode TPart::GetAst([[maybe_unused]] TAstContext& ctx) const {
    TAstNode node = TAstNode::MakeObject("TPart");
    [[maybe_unused]] const std::string owner_identity = ctx.IdentityOf(this);
    AddValueField(node, "bonus", Bonus_);
    return node;
}

TAbility::TAbility() : TAbility(Create({})) {}

TAbility::TInitialValues TAbility::ResolveInitialValues(const TInitialValues& initial) {
    TAbility result(TUninitialized{}, initial.Value ? *initial.Value : TGeneratedConstruction::Blank<int>());
    [[maybe_unused]] auto& self = result;
    (void)initial;
    result.Value_ = initial.Value ? *initial.Value : int{};
    return TInitialValues{.Value = std::move(result.Value_), };
}

TAbility TAbility::Create(const TInitialValues& initial) {
    [[maybe_unused]] auto values = ResolveInitialValues(initial);
    TAbility result(TUninitialized{}, std::move(*values.Value));
    return result;
}

int TAbility::Modifier() const {
    [[maybe_unused]] const auto& self = *this;
    return TRuleInteger::Divide(TRuleInteger::Subtract((std::as_const(self)).Value(), 10), 2);
}

TAstNode TAbility::GetAst([[maybe_unused]] TAstContext& ctx) const {
    TAstNode node = TAstNode::MakeObject("TAbility");
    [[maybe_unused]] const std::string owner_identity = ctx.IdentityOf(this);
    AddValueField(node, "value", Value_);
    return node;
}

TStats::TStats() : TStats(Create({})) {}

TStats::TInitialValues TStats::ResolveInitialValues(const TInitialValues& initial) {
    TStats result(TUninitialized{}, initial.Health ? *initial.Health : TGeneratedConstruction::Blank<TBoundedQuantity>(), initial.Base ? *initial.Base : TGeneratedConstruction::Blank<int>(), initial.Level ? *initial.Level : TGeneratedConstruction::Blank<int>(), initial.PerLevel ? *initial.PerLevel : TGeneratedConstruction::Blank<int>(), initial.Total ? *initial.Total : TGeneratedConstruction::Blank<int>(), initial.Part ? *initial.Part : TGeneratedConstruction::Blank<::TPart>(), initial.Boosted ? *initial.Boosted : TGeneratedConstruction::Blank<int>(), initial.Ability ? *initial.Ability : TGeneratedConstruction::Blank<::TAbility>(), initial.ModBoost ? *initial.ModBoost : TGeneratedConstruction::Blank<int>());
    [[maybe_unused]] auto& self = result;
    (void)initial;
    result.Base_ = initial.Base ? *initial.Base : int{};
    result.Level_ = initial.Level ? *initial.Level : 1;
    result.PerLevel_ = initial.PerLevel ? *initial.PerLevel : int{};
    result.Health_ = initial.Health ? *initial.Health : TBoundedQuantity(TRuleInteger::Add((std::as_const(self)).Base(), TRuleInteger::Multiply((std::as_const(self)).Level(), (std::as_const(self)).PerLevel())));
    result.Total_ = initial.Total ? *initial.Total : TRuleInteger::Add((std::as_const(self)).Base(), (std::as_const(self)).PerLevel());
    result.Part_ = initial.Part ? *initial.Part : ::TPart::Create({});
    result.Boosted_ = initial.Boosted ? *initial.Boosted : TRuleInteger::Add(((std::as_const(self)).Part()).Bonus(), (std::as_const(self)).Base());
    result.Ability_ = initial.Ability ? *initial.Ability : ::TAbility::Create({});
    result.ModBoost_ = initial.ModBoost ? *initial.ModBoost : TRuleInteger::Add(((std::as_const(self)).Ability()).Modifier(), (std::as_const(self)).Base());
    return TInitialValues{.Health = std::move(result.Health_), .Base = std::move(result.Base_), .Level = std::move(result.Level_), .PerLevel = std::move(result.PerLevel_), .Total = std::move(result.Total_), .Part = std::move(result.Part_), .Boosted = std::move(result.Boosted_), .Ability = std::move(result.Ability_), .ModBoost = std::move(result.ModBoost_), };
}

TStats TStats::Create(const TInitialValues& initial) {
    [[maybe_unused]] auto values = ResolveInitialValues(initial);
    TStats result(TUninitialized{}, std::move(*values.Health), std::move(*values.Base), std::move(*values.Level), std::move(*values.PerLevel), std::move(*values.Total), std::move(*values.Part), std::move(*values.Boosted), std::move(*values.Ability), std::move(*values.ModBoost));
    return result;
}

TAstNode TStats::GetAst([[maybe_unused]] TAstContext& ctx) const {
    TAstNode node = TAstNode::MakeObject("TStats");
    [[maybe_unused]] const std::string owner_identity = ctx.IdentityOf(this);
    if (!owner_identity.empty() && static_cast<const void*>(&Health_) != static_cast<const void*>(this)) {
        ctx.RegisterIdentity(&Health_, owner_identity + ".health");
    }
    AddOwnedObject(node, "health", Health_, ctx);
    AddValueField(node, "base", Base_);
    AddValueField(node, "level", Level_);
    AddValueField(node, "per_level", PerLevel_);
    AddValueField(node, "total", Total_);
    if (!owner_identity.empty() && static_cast<const void*>(&Part_) != static_cast<const void*>(this)) {
        ctx.RegisterIdentity(&Part_, owner_identity + ".part");
    }
    AddOwnedObject(node, "part", Part_, ctx);
    AddValueField(node, "boosted", Boosted_);
    if (!owner_identity.empty() && static_cast<const void*>(&Ability_) != static_cast<const void*>(this)) {
        ctx.RegisterIdentity(&Ability_, owner_identity + ".ability");
    }
    AddOwnedObject(node, "ability", Ability_, ctx);
    AddValueField(node, "mod_boost", ModBoost_);
    return node;
}

void RegisterComputedAbilities(TAbilityCatalog& catalog) {
    (void)catalog;
}

void RegisterComputedRules(TAbilityRules& rules) {
    RegisterComputedAbilities(rules.Catalog);
}


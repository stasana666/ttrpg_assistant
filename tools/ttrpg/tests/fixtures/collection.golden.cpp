// AUTO-GENERATED FROM collection.ttrpg -- DO NOT EDIT.
#include <collection.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/actions/ability.h>

#include <stdexcept>
#include <string>
#include <utility>

TThing::TThing() : TThing(Create({})) {}

TThing::TInitialValues TThing::ResolveInitialValues(const TInitialValues& initial) {
    TThing result(TUninitialized{}, initial.X ? *initial.X : TGeneratedConstruction::Blank<int>());
    [[maybe_unused]] auto& self = result;
    (void)initial;
    result.X_ = initial.X ? *initial.X : int{};
    return TInitialValues{.X = std::move(result.X_), };
}

TThing TThing::Create(const TInitialValues& initial) {
    [[maybe_unused]] auto values = ResolveInitialValues(initial);
    TThing result(TUninitialized{}, std::move(*values.X));
    return result;
}

TAstNode TThing::GetAst([[maybe_unused]] TAstContext& ctx) const {
    TAstNode node = TAstNode::MakeObject("TThing");
    [[maybe_unused]] const std::string owner_identity = ctx.IdentityOf(this);
    AddValueField(node, "x", X_);
    return node;
}

THolder::THolder() : THolder(Create({})) {}

THolder::TInitialValues THolder::ResolveInitialValues(const TInitialValues& initial) {
    THolder result(TUninitialized{}, initial.Count ? *initial.Count : TGeneratedConstruction::Blank<int>(), initial.Things ? *initial.Things : TGeneratedConstruction::Blank<TIdCollection<::TThing>>());
    [[maybe_unused]] auto& self = result;
    (void)initial;
    result.Count_ = initial.Count ? *initial.Count : 0;
    result.Things_ = initial.Things ? *initial.Things : TIdCollection<TThing>{};
    return TInitialValues{.Count = std::move(result.Count_), .Things = std::move(result.Things_), };
}

THolder THolder::Create(const TInitialValues& initial) {
    [[maybe_unused]] auto values = ResolveInitialValues(initial);
    THolder result(TUninitialized{}, std::move(*values.Count), std::move(*values.Things));
    return result;
}

TAstNode THolder::GetAst([[maybe_unused]] TAstContext& ctx) const {
    TAstNode node = TAstNode::MakeObject("THolder");
    [[maybe_unused]] const std::string owner_identity = ctx.IdentityOf(this);
    AddValueField(node, "count", Count_);
    {
        TAstNode container = TAstNode::MakeObject("container");
        AddValueField(container, "next_id", Things_.NextId().Value);
        for (auto entry : Things_) {
            if (!owner_identity.empty()) {
                ctx.RegisterIdentity(&*entry, owner_identity + ".things." + std::to_string(entry.Id().Value));
            }
            AddOwnedObject(container, std::to_string(entry.Id().Value), *entry, ctx);
        }
        node.AddChild("things", std::move(container));
    }
    return node;
}

void RegisterCollectionAbilities(TAbilityCatalog& catalog) {
    (void)catalog;
}

void RegisterCollectionRules(TAbilityRules& rules) {
    RegisterCollectionAbilities(rules.Catalog);
}


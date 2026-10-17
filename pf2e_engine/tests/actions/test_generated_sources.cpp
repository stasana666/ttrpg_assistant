#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/rules.h>
#include <pf2e_engine/inventory/generated_sources.h>

#include "../test_lib/battle_fixture.h"
#include "../test_lib/state_diff.h"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

TAbilitySourcePath WeaponSource(std::size_t id)
{
    return TAbilitySourcePath{}.Field("weapons").Entry(id).Field("features").Entry(0);
}

class TGeneratedSourcesTest : public TBattleFixture {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedSourcesRules(*rules);
        StartBattle(std::move(rules), 8);
    }

    TPlayer& Add(std::string name, TCreature creature)
    {
        const int id = static_cast<int>(creatures_.size());
        return Join(std::move(creature), id == 0 ? 0 : 1, {id, 0}, std::move(name));
    }

    TPlayer& AddTarget(std::string name)
    {
        return Add(std::move(name), TCreature::Create({
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 100}),
                .Current = 100,
            }),
        }));
    }

    TAbilityOption Option(TPlayer& actor, const TAbilitySourcePath& source)
    {
        for (const auto& option : Options("generated_sources.WeaponProbe", actor.GetId())) {
            if (option.Source == source) {
                return option;
            }
        }
        throw std::logic_error("missing source-bound probe option");
    }
};

TEST_F(TGeneratedSourcesTest, NamedValuesPreserveTraitPayloadsAndTypedQueryArguments)
{
    const auto light = CreateProbeLight();
    const auto heavy = CreateProbeHeavy();
    const auto profile = TWeaponProbeProfile::Create({});
    EXPECT_EQ(light.Name(), heavy.Name());
    EXPECT_EQ(light.BaseDiceSize(), 4);
    EXPECT_EQ(heavy.BaseDiceSize(), 12);
    EXPECT_TRUE(light.Traits().Has(EWeaponTraitKind::Agile));
    EXPECT_FALSE(heavy.Traits().Has(EWeaponTraitKind::Agile));
    ASSERT_NE(light.Traits().Get<TWeaponTraitThrown>(), nullptr);
    EXPECT_EQ(light.Traits().Get<TWeaponTraitThrown>()->RangeFeet, 2);
    ASSERT_NE(heavy.Traits().Get<TWeaponTraitFatal>(), nullptr);
    EXPECT_EQ(heavy.Traits().Get<TWeaponTraitFatal>()->Die, EDieSize::D10);
    EXPECT_EQ(profile.Bonus(light), 5);
    EXPECT_EQ(profile.Bonus(heavy), 8);
    EXPECT_FALSE(profile.IsFatalD10(light));
    EXPECT_TRUE(profile.IsFatalD10(heavy));
    EXPECT_EQ(profile.Bonus(TWeapon::Create({
        .DamageType = EDamageType::Slashing,
        .Category = EWeaponCategory::Unarmed,
    })), 0);
}

TEST_F(TGeneratedSourcesTest, NamedLoadoutsCopyValuesAndCollectionQueriesReadCurrentContents)
{
    auto first = CreateProbeLoadout();
    const auto second = CreateProbeLoadout();
    const auto before = ObjectFields(first);
    EXPECT_EQ(first.MaximumReach(), 3);
    EXPECT_EQ(first.TotalDiceSize(), 20);
    EXPECT_EQ(ObjectFields(first), before);
    EXPECT_EQ(TWeaponProbeProfile::Create({}).MaximumReach(), 0);
    EXPECT_EQ(TWeaponProbeProfile::Create({}).TotalDiceSize(), 0);

    auto context = journal_->ExecutionContext();
    auto weapon = context.Project(first.Weapons(), [](TIdCollection<TWeapon>& values) {
        return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
    });
    auto reach = context.Project(weapon, [](TWeapon& value) { return value.Reach(); });
    context.AssignGuarded(reach, 7, weapon, "reach");

    EXPECT_EQ(first.MaximumReach(), 7);
    EXPECT_EQ(second.MaximumReach(), 3);
    EXPECT_EQ(first.Weapons()->Find(TId<TWeapon>{2})->Reach(), 1);
    EXPECT_EQ(CreateProbeLight().Reach().Get(), 1);
}

TEST_F(TGeneratedSourcesTest, JournalDistinguishesPreviousCollectionIdsAndAllocationState)
{
    const auto snapshot = [](const TCreature& creature, const TTransformator& journal) {
        TAstContext context;
        context.RegisterIdentity(&creature, "creature");
        auto node = TAstNode::MakeObject("equipment_replacement");
        AddOwnedObject(node, "creature", creature, context);
        AddOwnedObject(node, "journal", journal, context);
        return node;
    };
    const auto weapons = [](std::size_t id, std::size_t next) {
        TIdCollection<TWeapon> result;
        result.RestoreEntry(TId<TWeapon>{id}, CreateProbeLight());
        result.RestoreNextId(TId<TWeapon>{next});
        return result;
    };
    const auto& catalog = battle_->AbilityRules().Catalog;
    for (const auto& [other_id, other_next] :
         std::vector<std::pair<std::size_t, std::size_t>>{{4, 7}, {2, 9}}) {
        SCOPED_TRACE("other id=" + std::to_string(other_id)
                     + " next=" + std::to_string(other_next));
        auto first = TCreature::Create({.Weapons = weapons(2, 7)});
        auto second = TCreature::Create({.Weapons = weapons(other_id, other_next)});
        TTransformator first_journal(io_);
        TTransformator second_journal(io_);
        auto first_context = first_journal.ExecutionContext();
        auto second_context = second_journal.ExecutionContext();
        const auto first_start = first_context.GetState();
        const auto second_start = second_context.GetState();
        const auto first_before = snapshot(first, first_journal);
        const auto second_before = snapshot(second, second_journal);
        const auto first_grants = CollectAbilities(first, catalog).Grants();
        const auto second_grants = CollectAbilities(second, catalog).Grants();
        ASSERT_EQ(first_grants.size(), 1u);
        ASSERT_EQ(second_grants.size(), 1u);

        TIdCollection<TWeapon> replacement;
        replacement.Add(CreateProbeHeavy());
        first_context.AssignGuarded(first.Weapons(), replacement,
                                    TGuarded<TCreature>(first), "weapons");
        second_context.AssignGuarded(second.Weapons(), replacement,
                                     TGuarded<TCreature>(second), "weapons");

        TAstContext first_ast_context;
        TAstContext second_ast_context;
        first_ast_context.RegisterIdentity(&first, "creature");
        second_ast_context.RegisterIdentity(&second, "creature");
        EXPECT_EQ(first.GetAst(first_ast_context), second.GetAst(second_ast_context));
        EXPECT_NE(snapshot(first, first_journal), snapshot(second, second_journal));

        first_context.Undo(first_start);
        second_context.Undo(second_start);
        EXPECT_EQ(snapshot(first, first_journal), first_before);
        EXPECT_EQ(snapshot(second, second_journal), second_before);
        EXPECT_FALSE(first.Weapons()->Contains(TId<TWeapon>{0}));
        EXPECT_EQ(first.Weapons()->NextId().Value, 7u);
        EXPECT_EQ(second.Weapons()->NextId().Value, other_next);
        const auto first_restored = CollectAbilities(first, catalog).Grants();
        const auto second_restored = CollectAbilities(second, catalog).Grants();
        ASSERT_EQ(first_restored.size(), 1u);
        ASSERT_EQ(second_restored.size(), 1u);
        EXPECT_EQ(first_restored.front().Source, WeaponSource(2));
        EXPECT_EQ(second_restored.front().Source, WeaponSource(other_id));
        EXPECT_EQ(first_restored.front().Provenance, first_grants.front().Provenance);
        EXPECT_EQ(second_restored.front().Provenance, second_grants.front().Provenance);
        auto allocated = first.Weapons().Get();
        EXPECT_EQ(allocated.Add(CreateProbeHeavy()).Value, 7u);
    }
}

TEST_F(TGeneratedSourcesTest, SameNameWeaponsProduceSeparateOptionsAndOnlyChooseTheTarget)
{
    auto& actor = Add("Actor", CreateProbeFighter());
    AddTarget("First");
    AddTarget("Second");
    ASSERT_EQ(battle_->AvailableActions(actor).size(), 2u);
    const auto light = Option(actor, WeaponSource(0));
    const auto heavy = Option(actor, WeaponSource(1));
    EXPECT_EQ(light.DefinitionId, heavy.DefinitionId);
    EXPECT_FALSE(light.Provenance.empty());
    EXPECT_FALSE(heavy.Provenance.empty());
    Act(heavy, {.Choose = {"Second"}, .Rolls = {{12, 7}},
        .Changes = {{"Second.hitpoints.current", 85}, {"Actor.actions.count", 2}}});
    EXPECT_EQ(io_.Candidates(), (std::vector<std::string>{"Actor", "First", "Second"}));
    Act(light, {.Choose = {"First"}, .Rolls = {{4, 3}},
        .Changes = {{"First.hitpoints.current", 92}, {"Actor.actions.count", 1}}});
}

TEST_F(TGeneratedSourcesTest, RootFeatureCannotSatisfyTheDeclaredWeaponSource)
{
    TIdCollection<TFeatureSelection> features;
    features.Add(TFeatureSelection("generated_sources.ProbeTraining"));
    auto& actor = Add("Actor", TCreature::Create({
        .Hitpoints = THitPoints::Create({
            .Maximum = Statistic::Create({.Base = 100}),
            .Current = 100,
        }),
        .Features = features,
        .Actions = TResource(3),
    }));
    AddTarget("Target");
    const auto grants = CollectAbilities(*actor.GetCreature(), battle_->AbilityRules().Catalog);
    ASSERT_EQ(grants.Grants().size(), 1u);
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    const auto& grant = grants.Grants().front();
    const TAbilityOption forged{
        .ActorId = actor.GetId(),
        .ActorIdentity = &actor,
        .DefinitionId = grant.DefinitionId,
        .Source = grant.Source,
        .Provenance = grant.Provenance,
        .Action = battle_->AbilityRules().Executions.Require(grant.DefinitionId).Action,
    };
    EXPECT_EQ(battle_->Execute(forged), EAbilityExecutionResult::Unavailable);
    Act(forged, {.Result = EAbilityExecutionResult::Unavailable});
}

TEST_F(TGeneratedSourcesTest, NaturalWeaponUsesItsNearestProviderWithoutAnEquipmentFieldName)
{
    TIdCollection<TWeapon> natural;
    natural.Add(CreateProbeHeavy());
    auto& actor = Add("Actor", TCreature::Create({
        .Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 100}), .Current = 100}),
        .NaturalWeapons = natural,
        .Actions = TResource(3),
    }));
    AddTarget("Target");
    Act(Option(actor, TAbilitySourcePath{}.Field("natural_weapons").Entry(0).Field("features").Entry(0)),
        {.Choose = {"Target"}, .Rolls = {{12, 6}}, .Changes = {{"Target.hitpoints.current", 86}, {"Actor.actions.count", 2}}});
}

TEST_F(TGeneratedSourcesTest, NamedCreatureFactoriesOwnIndependentWeaponValues)
{
    auto& actor = Add("Actor", CreateProbeFighter());
    auto& copy = Add("Copy", CreateProbeFighter());
    EXPECT_NE(actor.GetCreature()->Id(), copy.GetCreature()->Id());
    const auto saved = Option(actor, WeaponSource(0));

    auto context = journal_->ExecutionContext();
    const auto initial = context.GetState();
    auto weapons = actor.GetCreature()->Weapons().Get();
    weapons.Remove(TId<TWeapon>{0});
    context.AssignGuarded(actor.GetCreature()->Weapons(), std::move(weapons),
                          TGuarded<TCreature>(*actor.GetCreature()), "weapons");
    EXPECT_EQ(battle_->AvailableActions(actor).size(), 1u);
    EXPECT_EQ(battle_->AvailableActions(copy).size(), 2u);
    EXPECT_EQ(copy.GetCreature()->Weapons()->Size(), 2u);
    Act(saved, {.Result = EAbilityExecutionResult::Unavailable});

    context.Undo(initial);
    const auto after = Option(actor, WeaponSource(0));
    EXPECT_EQ(after.Source, saved.Source);
    EXPECT_EQ(after.Provenance, saved.Provenance);
}

TEST_F(TGeneratedSourcesTest, RemovingTheSourceDuringASuspensionRejectsTheResumeBeforeRolling)
{
    auto& actor = Add("Actor", CreateProbeFighter());
    AddTarget("Target");
    io_.ExpectName(0, {}, "Target", true);
    const auto point = Pause([&]() { Execute(Option(actor, WeaponSource(0))); });
    TTransformator editor(io_);
    auto context = editor.ExecutionContext();
    const auto edit_start = context.GetState();
    auto weapons = actor.GetCreature()->Weapons().Get();
    weapons.Remove(TId<TWeapon>{0});
    context.AssignGuarded(actor.GetCreature()->Weapons(), std::move(weapons), TGuarded<TCreature>(*actor.GetCreature()),
                          "weapons");
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    context.Undo(edit_start);
    rng_.ExpectCall(4, 3);
    point.Resume();
    EXPECT_EQ(Read(1).Hitpoints().Current(), 92);
}

TEST_F(TGeneratedSourcesTest, SuspendedExecutionKeepsSourceValuesAndNextExecutionTakesNewSnapshot)
{
    auto& actor = Add("Actor", CreateProbeFighter());
    auto& target = AddTarget("Target");
    const auto option = Option(actor, WeaponSource(0));
    io_.ExpectName(0, {}, "Target", true);
    rng_.ExpectCall(4, 3);
    const auto point = Pause([&]() { battle_->Execute(option); });

    auto context = journal_->ExecutionContext();
    const auto initial = context.GetState();
    auto weapon = context.Project(actor.GetCreature()->Weapons(), [](TIdCollection<TWeapon>& values) {
        return TGuarded<TWeapon>(*values.Find(TId<TWeapon>{0}));
    });
    const auto original_weapon = ObjectFields(weapon.Get());
    auto die = context.Project(weapon, [](TWeapon& value) { return value.BaseDiceSize(); });
    auto traits = context.Project(weapon, [](TWeapon& value) { return value.Traits(); });
    const auto heavy = CreateProbeHeavy();
    context.AssignGuarded(die, 12, weapon, "base_dice_size");
    context.AssignGuarded(traits, heavy.Traits(), weapon, "traits");
    EXPECT_EQ(battle_->AvailableActions(actor).size(), 2u);

    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 92);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    io_.ExpectName(0, {}, "Target");
    rng_.ExpectCall(12, 6);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 79);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 1);
    context.Undo(initial);
    EXPECT_EQ(ObjectFields(*actor.GetCreature()->Weapons()->Find(TId<TWeapon>{0})), original_weapon);
}

}

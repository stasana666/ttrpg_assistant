#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/common/continuation.h>
#include <pf2e_engine/common/id_collection.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/inventory/weapon.h>

#include "../ast/ast_test_fixture.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

TAbilityCatalog MakeCatalog()
{
    TAbilityCatalog catalog;
    catalog.Register({"test.Strike", EAbilityKind::Action, {}});
    catalog.Register({"test.Counter", EAbilityKind::Reaction, {}});
    catalog.Register({"test.Training", EAbilityKind::Feature, {"test.Strike"}});
    catalog.Register({"test.Fighter", EAbilityKind::Feature,
                      {"test.Training", "test.Strike", "test.Counter"}});
    return catalog;
}

struct TTestProvider {
    TIdCollection<TFeatureSelection> Features;

    void VisitGrantedAbilities(TAbilityCollector& collector,
                               const TAbilitySourcePath& path = {}) const
    {
        for (auto entry : Features) {
            collector.Visit(*entry, path.Field("features").Entry(entry.Id().Value),
                            "TTestProvider.Features");
        }
    }
};

class TCallbackBlock final : public IActionBlock {
public:
    using FCallback = std::function<void(std::shared_ptr<TActionContext>)>;

    explicit TCallbackBlock(FCallback callback)
        : callback_(std::move(callback))
    {
    }

    void Apply(std::shared_ptr<TActionContext> ctx) override
    {
        continuation::Then(
            [this, ctx]() { callback_(ctx); },
            [ctx]() { ctx->next_block = nullptr; });
    }

private:
    FCallback callback_;
};

std::shared_ptr<const TAction> MakeAction(
    TAction::TResources resources, TCallbackBlock::FCallback callback = [](auto) {})
{
    TAction::TPipeline pipeline;
    pipeline.push_back(std::make_unique<TCallbackBlock>(std::move(callback)));
    return std::make_shared<TAction>(std::move(pipeline), std::move(resources), "test action");
}

TPlayer& Actor(TTestBattle& fixture, int id = 0)
{
    auto players = fixture.battle->GetIfPlayers([id](const auto* player) {
        return player->GetId() == id;
    });
    return *players.at(0);
}

std::shared_ptr<TActionContext> MakeContext(TTestBattle& fixture, TTransformator& transformator)
{
    return std::make_shared<TActionContext>(TActionContext{
        .battle = fixture.battle.get(),
        .dice_roller = &fixture.rng,
        .transformator = &transformator,
        .io_system = &fixture.io,
        .scheduler = nullptr,
        .effect_manager = nullptr,
    });
}

}

TEST(AbilityCatalogTest, RegistrationIsIdempotentAndRejectsConflicts)
{
    auto catalog = MakeCatalog();
    catalog.Register({"test.Strike", EAbilityKind::Action, {}});
    EXPECT_EQ(catalog.Require("test.Strike").Kind, EAbilityKind::Action);
    EXPECT_EQ(catalog.Find("missing"), nullptr);
    EXPECT_THROW(catalog.Require("missing"), std::invalid_argument);
    EXPECT_THROW(catalog.Register({"test.Strike", EAbilityKind::Reaction, {}}),
                 std::invalid_argument);
    EXPECT_THROW(catalog.Register({"", EAbilityKind::Action, {}}), std::invalid_argument);
    EXPECT_THROW(catalog.Register({"bad", static_cast<EAbilityKind>(42), {}}),
                 std::invalid_argument);
    EXPECT_THROW(catalog.Register({"bad", EAbilityKind::Action, {"test.Strike"}}),
                 std::invalid_argument);
}

TEST(AbilityCatalogTest, RecursiveGrantsDeduplicateWithinSourceAndPreserveProvenance)
{
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    const auto first = TAbilitySourcePath{}.Field("weapons").Entry(3);
    const auto second = TAbilitySourcePath{}.Field("weapons").Entry(9);
    collector.Visit(TFeatureSelection("test.Fighter"), first, "weapon.features");
    collector.Visit(TFeatureSelection("test.Fighter"), first, "weapon.features");
    collector.Visit(TFeatureSelection("test.Training"), second, "weapon.features");

    ASSERT_EQ(collector.Grants().size(), 3);
    EXPECT_EQ(collector.Grants()[0].DefinitionId, "test.Strike");
    EXPECT_EQ(collector.Grants()[0].Source, first);
    EXPECT_EQ(collector.Grants()[0].Provenance.size(), 2);
    EXPECT_EQ(collector.Grants()[1].Kind, EAbilityKind::Reaction);
    EXPECT_EQ(collector.Grants()[2].DefinitionId, "test.Strike");
    EXPECT_EQ(collector.Grants()[2].Source, second);
}

TEST(AbilityCatalogTest, FailedExpansionDoesNotPublishPartialGrants)
{
    auto catalog = MakeCatalog();
    catalog.Register({"test.Broken", EAbilityKind::Feature, {"test.Counter", "missing"}});
    catalog.Register({"test.CycleA", EAbilityKind::Feature, {"test.Strike", "test.CycleB"}});
    catalog.Register({"test.CycleB", EAbilityKind::Feature, {"test.CycleA"}});
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", {}, "initial");
    const auto initial = collector.Grants();

    EXPECT_THROW(collector.Grant("test.Broken", {}, "broken"), std::invalid_argument);
    EXPECT_EQ(collector.Grants(), initial);
    EXPECT_THROW(collector.Grant("test.CycleA", {}, "cycle"), std::invalid_argument);
    EXPECT_EQ(collector.Grants(), initial);
    EXPECT_THROW(collector.Visit(TFeatureSelection("test.Strike"), {}, "wrong_kind"),
                 std::invalid_argument);
    EXPECT_EQ(collector.Grants(), initial);
}

TEST(AbilityCatalogTest, RecollectionTracksSourceRemovalWithoutRenumberingRemainingSources)
{
    const auto catalog = MakeCatalog();
    TTestProvider provider;
    const auto first = provider.Features.Add(TFeatureSelection("test.Training"));
    const auto second = provider.Features.Add(TFeatureSelection("test.Training"));
    TAbilityCollector initial(catalog);
    initial.Visit(provider, TAbilitySourcePath{}.Field("weapon"), "creature.weapon");
    ASSERT_EQ(initial.Grants().size(), 2);

    provider.Features.Remove(first);
    TAbilityCollector current(catalog);
    current.Visit(provider, TAbilitySourcePath{}.Field("weapon"), "creature.weapon");
    ASSERT_EQ(current.Grants().size(), 1);
    EXPECT_EQ(current.Grants()[0].Source,
              TAbilitySourcePath{}.Field("weapon").Field("features").Entry(second.Value));
    EXPECT_EQ(current.Grants()[0], initial.Grants()[1]);
}

TEST(AbilityCatalogTest, SourcePathsCompareTypedSegments)
{
    const auto path = TAbilitySourcePath{}.Field("weapons").Entry(1).Field("features").Entry(4);
    EXPECT_TRUE(path.StartsWith(TAbilitySourcePath{}.Field("weapons").Entry(1)));
    EXPECT_FALSE(path.StartsWith(TAbilitySourcePath{}.Field("weapons").Entry(10)));
    EXPECT_FALSE(path.StartsWith(TAbilitySourcePath{}.Field("weapons[1]")));
    EXPECT_NE(TAbilitySourcePath{}.Field("1"), TAbilitySourcePath{}.Entry(1));
    EXPECT_NE(TAbilitySourcePath{}.Field("a.b"), TAbilitySourcePath{}.Field("a").Field("b"));
    EXPECT_THROW(TAbilitySourcePath{}.Field(""), std::invalid_argument);
}

TEST(AbilityCatalogTest, FeatureSelectionsRoundTripAndEmptySelectionDoesNotGrant)
{
    const TFeatureSelection selected("test.Fighter");
    EXPECT_EQ(TFeatureSelection::FromJson(selected.ToJson()), selected);
    EXPECT_EQ(selected.ToJson(), nlohmann::json("test.Fighter"));
    const TFeatureSelection empty;
    EXPECT_TRUE(empty.ToJson().is_null());
    EXPECT_EQ(TFeatureSelection::FromJson(nullptr), empty);
    EXPECT_THROW(TFeatureSelection::FromJson(""), std::invalid_argument);
    EXPECT_THROW(TFeatureSelection::FromJson(17), std::invalid_argument);

    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Visit(empty, {}, "empty");
    EXPECT_TRUE(collector.Grants().empty());
    TAstContext ast_context;
    EXPECT_NE(selected.GetAst(ast_context).PrettyPrint().find("test.Fighter"), std::string::npos);
}

TEST(AbilityExecutionTest, EnumerationIsPureAndMissingOrMismatchedExecutionsAreErrors)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    transformator.AddResource(actor.GetCreature()->ResourceFor(EResourceKind::Action), 3);
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", {}, "self");
    TAbilityExecutions executions;
    EXPECT_THROW(EnumerateActions(0, collector, executions, actor), std::invalid_argument);
    executions.Register("test.Strike", MakeAction({{EResourceKind::Action, 1}}));
    TAstContext before_context;
    const auto before = fixture->battle->GetAst(before_context);
    const auto options = EnumerateActions(0, collector, executions, actor);
    ASSERT_EQ(options.size(), 1);
    EXPECT_EQ(options[0].ActorId, 0);
    const TAbilityExecutions missing;
    EXPECT_THROW(ExecuteAbility(options[0], MakeContext(*fixture, transformator), actor,
                                collector, missing), std::invalid_argument);
    EXPECT_EQ(actor.GetCreature()->ResourceFor(EResourceKind::Action)->Count(), 3);
    TAstContext after_context;
    EXPECT_EQ(fixture->battle->GetAst(after_context), before);

    TAbilityExecutions mismatch;
    mismatch.Register("test.Strike", MakeAction({}), ETrigger::OnMove);
    EXPECT_THROW(EnumerateActions(0, collector, mismatch, actor), std::invalid_argument);
}

TEST(AbilityExecutionTest, StaleSourceAndChangedRequirementsRejectBeforeResourceConsumption)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    transformator.AddResource(actor.GetCreature()->ResourceFor(EResourceKind::Action), 3);
    const auto catalog = MakeCatalog();
    const auto path = TAbilitySourcePath{}.Field("weapons").Entry(8);
    TAbilityCollector initial(catalog);
    initial.Grant("test.Strike", path, "weapon");
    bool enabled = true;
    int calls = 0;
    TAbilityExecutions executions;
    executions.Register("test.Strike",
        MakeAction({{EResourceKind::Action, 1}}, [&calls](auto) { ++calls; }), {},
        [&enabled](const auto&, const auto&, const auto*) { return enabled; });
    const auto option = EnumerateActions(0, initial, executions, actor).at(0);
    TAbilityCollector removed(catalog);
    EXPECT_EQ(ExecuteAbility(option, MakeContext(*fixture, transformator), actor,
                             removed, executions), EAbilityExecutionResult::Unavailable);
    enabled = false;
    EXPECT_EQ(ExecuteAbility(option, MakeContext(*fixture, transformator), actor,
                             initial, executions), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(actor.GetCreature()->ResourceFor(EResourceKind::Action)->Count(), 3);
}

TEST(AbilityExecutionTest, ResourceCostsAreAggregatedAndRecheckedAtExecution)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    transformator.AddResource(actor.GetCreature()->ResourceFor(EResourceKind::Action), 3);
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", {}, "self");
    TAbilityExecutions executions;
    executions.Register("test.Strike",
        MakeAction({{EResourceKind::Action, 1}, {EResourceKind::Action, 2}}));
    const auto option = EnumerateActions(0, collector, executions, actor).at(0);
    transformator.ReduceResource(actor.GetCreature()->ResourceFor(EResourceKind::Action), 1);
    EXPECT_TRUE(EnumerateActions(0, collector, executions, actor).empty());
    EXPECT_EQ(ExecuteAbility(option, MakeContext(*fixture, transformator), actor,
                             collector, executions), EAbilityExecutionResult::Unavailable);
    EXPECT_THROW(option.Action->Apply(MakeContext(*fixture, transformator), actor),
                 TAbilityUnavailable);
    EXPECT_EQ(actor.GetCreature()->ResourceFor(EResourceKind::Action)->Count(), 2);
    EXPECT_THROW(MakeAction({{EResourceKind::Action, -1}}), std::invalid_argument);
    EXPECT_THROW(MakeAction({{EResourceKind::Action, std::numeric_limits<int>::max()},
                             {EResourceKind::Action, 1}}), std::invalid_argument);
}

TEST(AbilityExecutionTest, SourceWeaponBindingUsesOwnedSnapshotAndCannotBeOverriddenByVariables)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", TAbilitySourcePath{}.Field("weapons").Entry(0), "weapon");
    const auto weapon_id = TGameObjectIdManager::Instance().Register("weapon");
    const auto source_id = TGameObjectIdManager::Instance().Register("source");
    const auto first_weapon = *std::as_const(actor).GetCreature()->Weapons().begin();
    const auto weapon = std::make_shared<const TWeapon>(*first_weapon);
    int calls = 0;
    TAbilityExecutions executions;
    executions.Register("test.Strike", MakeAction({}, [&, weapon](auto ctx) {
        EXPECT_EQ(std::get<const TWeapon*>(ctx->game_object_registry->GetGameObjectPtr(weapon_id)),
                  weapon.get());
        EXPECT_EQ(std::get<const TWeapon*>(ctx->game_object_registry->GetGameObjectPtr(source_id)),
                  weapon.get());
        ++calls;
    }));
    const auto option = EnumerateActions(0, collector, executions, actor).at(0);
    EXPECT_EQ(ExecuteAbility(option, MakeContext(*fixture, transformator), actor, collector,
                             executions, {.SourceWeapon = weapon}), EAbilityExecutionResult::Applied);
    EXPECT_EQ(calls, 1);

    TAction::TVariables variables{{weapon_id, std::make_shared<TWeapon>(*weapon)}};
    EXPECT_THROW(TAction({}, {}, "reserved", std::move(variables)), std::invalid_argument);
}

TEST(AbilityExecutionTest, ReactionCandidatesBindBothActorsWithoutExecuting)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& reactor = Actor(*fixture);
    auto& trigger_actor = Actor(*fixture, 1);
    TTransformator transformator(fixture->io);
    transformator.AddResource(reactor.GetCreature()->ResourceFor(EResourceKind::Reaction), 1);
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Counter", {}, "feature");
    int calls = 0;
    TAbilityExecutions executions;
    executions.Register("test.Counter", MakeAction({{EResourceKind::Reaction, 1}},
        [&calls](auto) { ++calls; }), ETrigger::OnMove);
    const TTriggerContext trigger{ETrigger::OnMove, &trigger_actor};
    const auto options = EnumerateReactions(0, collector, executions, reactor, trigger);
    ASSERT_EQ(options.size(), 1);
    EXPECT_EQ(options[0].ActorId, reactor.GetId());
    ASSERT_TRUE(options[0].Trigger.has_value());
    EXPECT_EQ(options[0].Trigger->ActorId, trigger_actor.GetId());
    EXPECT_EQ(calls, 0);
    EXPECT_TRUE(EnumerateActions(0, collector, executions, reactor).empty());
    EXPECT_EQ(ExecuteAbility(options[0], MakeContext(*fixture, transformator), reactor,
                             collector, executions), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(ExecuteAbility(options[0], MakeContext(*fixture, transformator), reactor,
                             collector, executions, {.TriggerActor = &trigger_actor}),
              EAbilityExecutionResult::Applied);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(reactor.GetCreature()->ResourceFor(EResourceKind::Reaction)->Count(), 0);
}

TEST(AbilityExecutionTest, SamePlayerIdInDifferentBattleDoesNotReuseActorBinding)
{
    auto first = MakeTwoWarriorBattle();
    auto second = MakeTwoWarriorBattle();
    auto& actor = Actor(*first);
    auto& foreign_actor = Actor(*second);
    TTransformator transformator(second->io);
    transformator.AddResource(foreign_actor.GetCreature()->ResourceFor(EResourceKind::Action), 3);
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", {}, "self");
    TAbilityExecutions executions;
    executions.Register("test.Strike", MakeAction({}));
    const auto option = EnumerateActions(0, collector, executions, actor).at(0);

    EXPECT_EQ(actor.GetId(), foreign_actor.GetId());
    EXPECT_EQ(ExecuteAbility(option, MakeContext(*second, transformator), foreign_actor,
                             collector, executions), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(foreign_actor.GetCreature()->ResourceFor(EResourceKind::Action)->Count(), 3);
}

TEST(AbilityExecutionTest, SuspendedPipelineKeepsDefinitionAliveAndChargesOnlyOnce)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    transformator.AddResource(actor.GetCreature()->ResourceFor(EResourceKind::Action), 3);
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", {}, "self");
    int resumed = 0;
    auto action = MakeAction({{EResourceKind::Action, 1}}, [&resumed](auto ctx) {
        throw TSavepointStackUnwind(ctx->transformator->CurrentState(), [&resumed]() {
            ++resumed;
        });
    });
    const std::weak_ptr<const TAction> weak_action = action;
    TAbilityExecutions executions;
    executions.Register("test.Strike", action);
    auto option = EnumerateActions(0, collector, executions, actor).at(0);
    std::optional<TSavepointStackUnwind> suspended;
    try {
        ExecuteAbility(option, MakeContext(*fixture, transformator), actor, collector, executions);
        FAIL() << "action did not suspend";
    } catch (const TSavepointStackUnwind& save_point) {
        suspended = save_point;
    }
    option.Action.reset();
    action.reset();
    executions = TAbilityExecutions{};
    EXPECT_FALSE(weak_action.expired());
    ASSERT_TRUE(suspended.has_value());
    suspended->Revert(transformator);
    suspended->Resume();
    EXPECT_EQ(resumed, 1);
    EXPECT_EQ(actor.GetCreature()->ResourceFor(EResourceKind::Action)->Count(), 2);
    suspended.reset();
    EXPECT_TRUE(weak_action.expired());
}

TEST(AbilityExecutionTest, RepeatedSuspensionRevalidatesSourceBeforeInnerResumeCallback)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    transformator.AddResource(actor.GetCreature()->ResourceFor(EResourceKind::Action), 3);
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", {}, "self");
    int resumed = 0;
    bool present = true;
    TAbilityExecutions executions;
    executions.Register("test.Strike", MakeAction({{EResourceKind::Action, 1}}, [&resumed](auto ctx) {
        throw TSavepointStackUnwind(ctx->transformator->CurrentState(), [ctx, &resumed]() {
            ++resumed;
            throw TSavepointStackUnwind(ctx->transformator->CurrentState(), [&resumed]() {
                ++resumed;
            });
        });
    }));
    const auto option = EnumerateActions(0, collector, executions, actor).at(0);
    std::optional<TSavepointStackUnwind> first;
    try {
        ExecuteAbility(option, MakeContext(*fixture, transformator), actor, collector, executions,
                       {.ValidateSource = [&present]() { return present; }});
        FAIL() << "action did not suspend";
    } catch (const TSavepointStackUnwind& save_point) {
        first = save_point;
    }
    ASSERT_TRUE(first.has_value());
    std::optional<TSavepointStackUnwind> second;
    try {
        first->Resume();
        FAIL() << "action did not suspend again";
    } catch (const TSavepointStackUnwind& save_point) {
        second = save_point;
    }
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(resumed, 1);
    present = false;
    EXPECT_THROW(second->Resume(), TAbilityUnavailable);
    EXPECT_EQ(resumed, 1);
    EXPECT_EQ(actor.GetCreature()->ResourceFor(EResourceKind::Action)->Count(), 2);
}

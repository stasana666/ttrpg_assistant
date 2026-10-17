#include <pf2e_engine/creature.h>

#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/common/id_collection.h>

#include "../ast/ast_test_fixture.h"
#include "../test_lib/journal_setup.h"

#include <gtest/gtest.h>

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

TAction::TBoundResourceCost BindResource(
    std::string key, int count,
    const TResource& (TCreature::*read)() const,
    TGuarded<TResource> (TCreature::*write)())
{
    return {
        .Key = std::move(key),
        .Count = count,
        .Read = [read](const TPlayer& actor) -> const TResource& {
            return std::invoke(read, *actor.GetCreature());
        },
        .Write = [write](TPlayer& actor) {
            return std::invoke(write, *actor.GetCreature());
        },
    };
}

TAction::TBoundResourceCost ActionCost(int count)
{
    return BindResource("self.actions", count, &TCreature::Actions, &TCreature::Actions);
}

std::shared_ptr<const TAction> MakeAction(TAction::TBoundResources costs, TAction::FBody body = [](auto) {})
{
    return std::make_shared<TAction>(std::move(body), std::move(costs), "test action");
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
    ASSERT_EQ(collector.Features().size(), 3);
    EXPECT_EQ(collector.Features()[0].DefinitionId, "test.Fighter");
    EXPECT_EQ(collector.Features()[1].DefinitionId, "test.Training");
    EXPECT_EQ(collector.Features()[1].Source, first);
    EXPECT_EQ(collector.Features()[2].Source, second);
}

TEST(AbilityCatalogTest, FailedExpansionDoesNotPublishPartialGrants)
{
    auto catalog = MakeCatalog();
    catalog.Register({"test.Broken", EAbilityKind::Feature, {"test.Counter", "missing"}});
    catalog.Register({"test.CycleA", EAbilityKind::Feature, {"test.Strike", "test.CycleB"}});
    catalog.Register({"test.CycleB", EAbilityKind::Feature, {"test.CycleA"}});
    TAbilityCollector collector(catalog);
    collector.Grant("test.Training", {}, "initial");
    const auto initial = collector.Grants();
    const auto initial_features = collector.Features();

    EXPECT_THROW(collector.Grant("test.Broken", {}, "broken"), std::invalid_argument);
    EXPECT_EQ(collector.Grants(), initial);
    EXPECT_EQ(collector.Features(), initial_features);
    EXPECT_THROW(collector.Grant("test.CycleA", {}, "cycle"), std::invalid_argument);
    EXPECT_EQ(collector.Grants(), initial);
    EXPECT_EQ(collector.Features(), initial_features);
    EXPECT_THROW(collector.Visit(TFeatureSelection("test.Strike"), {}, "wrong_kind"),
                 std::invalid_argument);
    EXPECT_EQ(collector.Grants(), initial);
    EXPECT_EQ(collector.Features(), initial_features);
}

TEST(AbilityCatalogTest, TerminalFreeFeaturesRetainSourceAndAllProvenance)
{
    TAbilityCatalog catalog;
    catalog.Register({"test.Hook", EAbilityKind::Feature, {}});
    catalog.Register({"test.Left", EAbilityKind::Feature, {"test.Hook"}});
    catalog.Register({"test.Right", EAbilityKind::Feature, {"test.Hook"}});
    catalog.Register({"test.Both", EAbilityKind::Feature, {"test.Left", "test.Right"}});
    TTestProvider provider;
    const auto source = provider.Features.Add(TFeatureSelection("test.Both"));
    const auto collector = CollectAbilities(provider, catalog);
    EXPECT_TRUE(collector.Grants().empty());
    ASSERT_EQ(collector.Features().size(), 4);
    const auto& hook = collector.Features()[2];
    EXPECT_EQ(hook.DefinitionId, "test.Hook");
    EXPECT_EQ(hook.Kind, EAbilityKind::Feature);
    EXPECT_EQ(hook.Source, TAbilitySourcePath{}.Field("features").Entry(source.Value));
    EXPECT_EQ(hook.Provenance.size(), 2);
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

TEST(AbilityCatalogTest, EmptySelectionDoesNotGrant)
{
    const TFeatureSelection selected("test.Fighter");
    const TFeatureSelection empty;

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
    AddResource(transformator, actor.GetCreature()->Actions(), 3);
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", {}, "self");
    TAbilityExecutions executions;
    EXPECT_THROW(EnumerateActions(0, collector, executions, actor), std::invalid_argument);
    executions.Register("test.Strike", MakeAction({ActionCost(1)}));
    TAstContext before_context;
    const auto before = fixture->battle->GetAst(before_context);
    const auto options = EnumerateActions(0, collector, executions, actor);
    ASSERT_EQ(options.size(), 1);
    EXPECT_EQ(options[0].ActorId, 0);
    const TAbilityExecutions missing;
    EXPECT_THROW(ExecuteAbility(options[0], MakeContext(*fixture, transformator), actor,
                                collector, missing), std::invalid_argument);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    TAstContext after_context;
    EXPECT_EQ(fixture->battle->GetAst(after_context), before);
}

TEST(AbilityExecutionTest, StaleSourceAndChangedRequirementsRejectBeforeResourceConsumption)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    AddResource(transformator, actor.GetCreature()->Actions(), 3);
    const auto catalog = MakeCatalog();
    const auto path = TAbilitySourcePath{}.Field("weapons").Entry(8);
    TAbilityCollector initial(catalog);
    initial.Grant("test.Strike", path, "weapon");
    bool enabled = true;
    int calls = 0;
    TAbilityExecutions executions;
    executions.Register("test.Strike", std::make_shared<const TAbilityExecution>(TAbilityExecution{
        .Action = MakeAction({ActionCost(1)}, [&calls](auto) { ++calls; }),
        .Requirement = [&enabled](const auto&, const auto&, const auto&) { return enabled; },
    }));
    const auto option = EnumerateActions(0, initial, executions, actor).at(0);
    TAbilityCollector removed(catalog);
    EXPECT_EQ(ExecuteAbility(option, MakeContext(*fixture, transformator), actor,
                             removed, executions), EAbilityExecutionResult::Unavailable);
    enabled = false;
    EXPECT_EQ(ExecuteAbility(option, MakeContext(*fixture, transformator), actor,
                             initial, executions), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
}

TEST(AbilityExecutionTest, ResourceCostsAreAggregatedAndRecheckedAtExecution)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    AddResource(transformator, actor.GetCreature()->Actions(), 3);
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", {}, "self");
    TAbilityExecutions executions;
    executions.Register("test.Strike",
        MakeAction({ActionCost(1), ActionCost(2)}));
    const auto option = EnumerateActions(0, collector, executions, actor).at(0);
    ReduceResource(transformator, actor.GetCreature()->Actions(), 1);
    EXPECT_TRUE(EnumerateActions(0, collector, executions, actor).empty());
    EXPECT_EQ(ExecuteAbility(option, MakeContext(*fixture, transformator), actor,
                             collector, executions), EAbilityExecutionResult::Unavailable);
    EXPECT_THROW(option.Action->Apply(MakeContext(*fixture, transformator), actor),
                 TAbilityUnavailable);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
}

TEST(AbilityExecutionTest, SamePlayerIdInDifferentBattleDoesNotReuseActorBinding)
{
    auto first = MakeTwoWarriorBattle();
    auto second = MakeTwoWarriorBattle();
    auto& actor = Actor(*first);
    auto& foreign_actor = Actor(*second);
    TTransformator transformator(second->io);
    AddResource(transformator, foreign_actor.GetCreature()->Actions(), 3);
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", {}, "self");
    TAbilityExecutions executions;
    executions.Register("test.Strike", MakeAction({}));
    const auto option = EnumerateActions(0, collector, executions, actor).at(0);

    EXPECT_EQ(actor.GetId(), foreign_actor.GetId());
    EXPECT_EQ(ExecuteAbility(option, MakeContext(*second, transformator), foreign_actor,
                             collector, executions), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(foreign_actor.GetCreature()->Actions()->Count(), 3);
}

TEST(AbilityExecutionTest, SuspendedActionKeepsDefinitionAliveAndChargesOnlyOnce)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    AddResource(transformator, actor.GetCreature()->Actions(), 3);
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", {}, "self");
    int resumed = 0;
    auto action = MakeAction({ActionCost(1)}, [&resumed](auto ctx) {
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
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    suspended.reset();
    EXPECT_TRUE(weak_action.expired());
}

TEST(AbilityExecutionTest, RepeatedSuspensionRevalidatesSourceBeforeInnerResumeCallback)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    AddResource(transformator, actor.GetCreature()->Actions(), 3);
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", {}, "self");
    int resumed = 0;
    bool present = true;
    TAbilityExecutions executions;
    executions.Register("test.Strike", MakeAction({ActionCost(1)}, [&resumed](auto ctx) {
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
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
}

TEST(AbilityBattleTest, InvalidAndDuplicateIdentitiesDoNotChangeBattle)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    auto extra = MakeWarrior();
    auto moved_from = MakeWarrior();
    auto moved = std::move(moved_from);
    TAstContext before_context;
    const auto before = fixture->battle->GetAst(before_context);
    const auto add = [&](TCreature* creature, int id) {
        fixture->battle->AddPlayer(
            TPlayer(creature, TPlayerTeam{0}, TPlayerId{id}, "Invalid", ""), {2, 0});
    };
    EXPECT_THROW(add(nullptr, 2), std::invalid_argument);
    EXPECT_THROW(add(&extra, actor.GetId()), std::invalid_argument);
    EXPECT_THROW(add(actor.GetCreature(), 2), std::invalid_argument);
    EXPECT_THROW(add(&moved_from, 2), std::invalid_argument);
    EXPECT_NE(moved.Id().Value, 0);
    TAstContext after_context;
    EXPECT_EQ(fixture->battle->GetAst(after_context), before);
    EXPECT_EQ(fixture->rng.RemainingCalls(), 0);
}

TEST(AbilityBattleTest, FailedInitiativeRestoresMapAndPlayers)
{
    auto fixture = MakeTwoWarriorBattle();
    auto extra = MakeWarrior();
    TAstContext before_context;
    const auto before = fixture->battle->GetAst(before_context);
    EXPECT_THROW(fixture->battle->AddPlayer(
        TPlayer(&extra, TPlayerTeam{0}, TPlayerId{2}, "Extra", ""), {2, 0}), std::logic_error);
    TAstContext after_context;
    EXPECT_EQ(fixture->battle->GetAst(after_context), before);
    EXPECT_EQ(fixture->battle->BattleMap()->GetCell(2, 0).player, nullptr);
    fixture->rng.ExpectCall(20, 15);
    EXPECT_NO_THROW(fixture->battle->AddPlayer(
        TPlayer(&extra, TPlayerTeam{0}, TPlayerId{2}, "Extra", ""), {2, 0}));
    EXPECT_EQ(fixture->battle->GetIfPlayers([](const auto*) { return true; }).size(), 3);
}

TEST(GeneratedActionTest, RejectsInvalidBodiesAndResourceBindings)
{
    const auto body = [](auto) {};
    const auto cost = BindResource("self.hands", 1, &TCreature::Hands, &TCreature::Hands);
    EXPECT_THROW(TAction(TAction::FBody{}, {}, "empty body"), std::invalid_argument);
    auto invalid = cost;
    invalid.Key.clear();
    EXPECT_THROW(TAction(body, {invalid}, "empty key"), std::invalid_argument);
    invalid = cost;
    invalid.Read = {};
    EXPECT_THROW(TAction(body, {invalid}, "missing read"), std::invalid_argument);
    invalid = cost;
    invalid.Write = {};
    EXPECT_THROW(TAction(body, {invalid}, "missing write"), std::invalid_argument);
    invalid = cost;
    invalid.Count = -1;
    EXPECT_THROW(TAction(body, {invalid}, "negative cost"), std::invalid_argument);
    invalid.Count = std::numeric_limits<int>::max();
    EXPECT_THROW(TAction(body, {invalid, cost}, "overflow"), std::invalid_argument);
}

TEST(GeneratedActionTest, CustomResourceCostsAggregateAliasesAndEnumerationIsPure)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    AddResource(transformator, actor.GetCreature()->Hands(), 5);
    const auto before = transformator.CurrentState();
    const auto cost = BindResource("self.hands", 1, &TCreature::Hands, &TCreature::Hands);
    auto alias = cost;
    alias.Key = "alias.hands";
    int applied = 0;
    auto action = std::make_shared<TAction>([&](std::shared_ptr<TActionContext> ctx) {
        ++applied;
        EXPECT_EQ(ctx->actor, &actor);
    }, TAction::TBoundResources{cost, cost, alias}, "bound hands");
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", {}, "self");
    TAbilityExecutions executions;
    executions.Register("test.Strike", action);
    TAstContext before_context;
    const auto battle_before = fixture->battle->GetAst(before_context);
    TAstContext journal_before_context;
    const auto journal_before = transformator.GetAst(journal_before_context);
    const auto options = EnumerateActions(actor.GetId(), collector, executions, actor);
    ASSERT_EQ(options.size(), 1);
    EXPECT_EQ(applied, 0);
    TAstContext after_context;
    EXPECT_EQ(fixture->battle->GetAst(after_context), battle_before);
    TAstContext journal_after_context;
    EXPECT_EQ(transformator.GetAst(journal_after_context), journal_before);
    EXPECT_EQ(ExecuteAbility(options[0], MakeContext(*fixture, transformator), actor,
                             collector, executions), EAbilityExecutionResult::Applied);
    EXPECT_EQ(applied, 1);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 0);
    EXPECT_TRUE(EnumerateActions(actor.GetId(), collector, executions, actor).empty());
    transformator.Undo(before);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 5);
}

TEST(GeneratedActionTest, ConflictingKeysAndAliasOverflowAreRejectedWithoutPayment)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    AddResource(transformator, actor.GetCreature()->Hands(), 3);
    AddResource(transformator, actor.GetCreature()->Actions(), 3);
    const auto hands = BindResource("self.shared", 1, &TCreature::Hands, &TCreature::Hands);
    const auto actions = BindResource("self.shared", 1, &TCreature::Actions, &TCreature::Actions);
    const TAction conflicting([](auto) {}, {hands, actions}, "conflicting key");
    EXPECT_THROW(conflicting.Check(actor), std::invalid_argument);
    EXPECT_THROW(conflicting.Consume(MakeContext(*fixture, transformator), actor),
                 std::invalid_argument);
    auto large = hands;
    large.Key = "large.alias";
    large.Count = std::numeric_limits<int>::max();
    const TAction overflow([](auto) {}, {hands, large}, "alias overflow");
    EXPECT_THROW(overflow.Check(actor), std::invalid_argument);
    EXPECT_THROW(overflow.Consume(MakeContext(*fixture, transformator), actor),
                 std::invalid_argument);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
}

TEST(GeneratedActionTest, BindingFailuresAndChangedBalancesDoNotPartiallyPay)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    AddResource(transformator, actor.GetCreature()->Hands(), 3);
    AddResource(transformator, actor.GetCreature()->Actions(), 3);
    const auto hands = BindResource("self.hands", 2, &TCreature::Hands, &TCreature::Hands);
    const auto actions = BindResource("self.actions", 2, &TCreature::Actions, &TCreature::Actions);
    auto invalid = actions;
    invalid.Write = hands.Write;
    TAstContext before_context;
    const auto before = transformator.GetAst(before_context);
    const TAction mismatch([](auto) {}, {hands, invalid}, "mismatched binding");
    EXPECT_TRUE(mismatch.Check(actor));
    EXPECT_THROW(mismatch.Consume(MakeContext(*fixture, transformator), actor),
                 std::invalid_argument);
    invalid.Write = [](TPlayer&) -> TGuarded<TResource> {
        throw std::runtime_error("resource binding unavailable");
    };
    const TAction failing([](auto) {}, {hands, invalid}, "failed binding");
    EXPECT_THROW(failing.Consume(MakeContext(*fixture, transformator), actor), std::runtime_error);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    TAstContext after_context;
    EXPECT_EQ(transformator.GetAst(after_context), before);

    const TAction valid([](auto) {}, {hands, actions}, "changed resources");
    EXPECT_TRUE(valid.Check(actor));
    ReduceResource(transformator, actor.GetCreature()->Actions(), 2);
    EXPECT_THROW(valid.Consume(MakeContext(*fixture, transformator), actor), TAbilityUnavailable);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 1);
}

TEST(GeneratedActionTest, PaidCostsComposeWithOuterRollbackAndBodyFailures)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    AddResource(transformator, actor.GetCreature()->Hands(), 3);
    AddResource(transformator, actor.GetCreature()->Actions(), 3);
    const TAction action([](auto) { throw std::runtime_error("body failed"); }, {
        BindResource("self.hands", 1, &TCreature::Hands, &TCreature::Hands),
        BindResource("self.actions", 2, &TCreature::Actions, &TCreature::Actions),
    }, "body failure");
    const auto before = transformator.CurrentState();
    auto execution = transformator.ExecutionContext();
    const auto failed_outer_method = [&]() {
        auto transaction = execution.Atomic();
        action.Consume(MakeContext(*fixture, transformator), actor);
        EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
        EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 1);
        throw std::runtime_error("outer method failed");
    };
    EXPECT_THROW(failed_outer_method(), std::runtime_error);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    EXPECT_THROW(action.Apply(MakeContext(*fixture, transformator), actor), std::runtime_error);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 1);
    transformator.Undo(before);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
}

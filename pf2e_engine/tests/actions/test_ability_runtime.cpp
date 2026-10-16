#include <pf2e_engine/mechanics/creature.h>

#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/common/continuation.h>
#include <pf2e_engine/common/id_collection.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/inventory/weapon.h>
#include <pf2e_engine/feat.h>

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

    explicit TCallbackBlock(FCallback callback, std::string name = {})
        : callback_(std::move(callback))
    {
        name_ = std::move(name);
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

std::unique_ptr<TTestBattle> MakeHookBattle(
    TCallbackBlock::FCallback hook_callback, TCallbackBlock::FCallback action_callback)
{
    auto fixture = std::make_unique<TTestBattle>();
    LoadFactory(fixture->factory);
    auto rules = std::make_shared<TAbilityRules>();
    rules->Catalog.Register({"test.HookedAction", EAbilityKind::Action, {}});
    rules->Catalog.Register({"test.ActionFeature", EAbilityKind::Feature, {"test.HookedAction"}});
    rules->Catalog.Register({"test.HookFeature", EAbilityKind::Feature, {}});
    TAction::TPipeline action_pipeline;
    action_pipeline.push_back(std::make_unique<TCallbackBlock>(
        std::move(action_callback), "probe"));
    rules->Executions.Register("test.HookedAction", std::make_shared<TAction>(
        std::move(action_pipeline), TAction::TResources{{EResourceKind::Action, 1}}, "probe action"));
    auto hook = std::make_shared<TCreatureFeat>();
    hook->name = "test hook";
    hook->blocks = {"probe"};
    hook->pipeline.push_back(std::make_unique<TCallbackBlock>(std::move(hook_callback)));
    rules->FeatureHooks.emplace("test.HookFeature", std::move(hook));

    TIdCollection<TFeatureSelection> features;
    features.Add(TFeatureSelection("test.ActionFeature"));
    TIdCollection<TFeatureSelection> hook_features;
    hook_features.Add(TFeatureSelection("test.HookFeature"));
    fixture->creatures.push_back(TCreature::Create({
        .Level = 1,
        .Class = TClass::Create({.Features = std::move(hook_features)}),
        .Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 10})}),
        .Features = std::move(features),
        .Actions = TResource(3),
    }));
    auto map_id = TGameObjectIdManager::Instance().Register("simple_battle_map");
    fixture->battle = std::make_unique<TBattle>(
        fixture->factory.Create<TBattleMap>(map_id), &fixture->rng, fixture->io, std::move(rules));
    fixture->rng.ExpectCall(20, 10);
    fixture->battle->AddPlayer(
        TPlayer(&fixture->creatures.front(), TPlayerTeam{0}, TPlayerId{0}, "Actor", ""), {0, 0});
    return fixture;
}

void RemoveHookFeature(TCreature& creature, TTransformator& transformator)
{
    auto context = transformator.ExecutionContext();
    context.AssignGuarded(creature.Class(), TClass::Create({}),
                          TGuarded<TCreature>(creature), "class");
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
    transformator.AddResource(ResourceFor(*actor.GetCreature(), EResourceKind::Action), 3);
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
    EXPECT_EQ(ResourceFor(*actor.GetCreature(), EResourceKind::Action)->Count(), 3);
    TAstContext after_context;
    EXPECT_EQ(fixture->battle->GetAst(after_context), before);
}

TEST(AbilityExecutionTest, StaleSourceAndChangedRequirementsRejectBeforeResourceConsumption)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    transformator.AddResource(ResourceFor(*actor.GetCreature(), EResourceKind::Action), 3);
    const auto catalog = MakeCatalog();
    const auto path = TAbilitySourcePath{}.Field("weapons").Entry(8);
    TAbilityCollector initial(catalog);
    initial.Grant("test.Strike", path, "weapon");
    bool enabled = true;
    int calls = 0;
    TAbilityExecutions executions;
    executions.Register("test.Strike",
        MakeAction({{EResourceKind::Action, 1}}, [&calls](auto) { ++calls; }),
        [&enabled](const auto&, const auto&) { return enabled; });
    const auto option = EnumerateActions(0, initial, executions, actor).at(0);
    TAbilityCollector removed(catalog);
    EXPECT_EQ(ExecuteAbility(option, MakeContext(*fixture, transformator), actor,
                             removed, executions), EAbilityExecutionResult::Unavailable);
    enabled = false;
    EXPECT_EQ(ExecuteAbility(option, MakeContext(*fixture, transformator), actor,
                             initial, executions), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(ResourceFor(*actor.GetCreature(), EResourceKind::Action)->Count(), 3);
}

TEST(AbilityExecutionTest, ResourceCostsAreAggregatedAndRecheckedAtExecution)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    transformator.AddResource(ResourceFor(*actor.GetCreature(), EResourceKind::Action), 3);
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", {}, "self");
    TAbilityExecutions executions;
    executions.Register("test.Strike",
        MakeAction({{EResourceKind::Action, 1}, {EResourceKind::Action, 2}}));
    const auto option = EnumerateActions(0, collector, executions, actor).at(0);
    transformator.ReduceResource(ResourceFor(*actor.GetCreature(), EResourceKind::Action), 1);
    EXPECT_TRUE(EnumerateActions(0, collector, executions, actor).empty());
    EXPECT_EQ(ExecuteAbility(option, MakeContext(*fixture, transformator), actor,
                             collector, executions), EAbilityExecutionResult::Unavailable);
    EXPECT_THROW(option.Action->Apply(MakeContext(*fixture, transformator), actor),
                 TAbilityUnavailable);
    EXPECT_EQ(ResourceFor(*actor.GetCreature(), EResourceKind::Action)->Count(), 2);
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

TEST(AbilityExecutionTest, SamePlayerIdInDifferentBattleDoesNotReuseActorBinding)
{
    auto first = MakeTwoWarriorBattle();
    auto second = MakeTwoWarriorBattle();
    auto& actor = Actor(*first);
    auto& foreign_actor = Actor(*second);
    TTransformator transformator(second->io);
    transformator.AddResource(ResourceFor(*foreign_actor.GetCreature(), EResourceKind::Action), 3);
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", {}, "self");
    TAbilityExecutions executions;
    executions.Register("test.Strike", MakeAction({}));
    const auto option = EnumerateActions(0, collector, executions, actor).at(0);

    EXPECT_EQ(actor.GetId(), foreign_actor.GetId());
    EXPECT_EQ(ExecuteAbility(option, MakeContext(*second, transformator), foreign_actor,
                             collector, executions), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(ResourceFor(*foreign_actor.GetCreature(), EResourceKind::Action)->Count(), 3);
}

TEST(AbilityExecutionTest, SuspendedPipelineKeepsDefinitionAliveAndChargesOnlyOnce)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    transformator.AddResource(ResourceFor(*actor.GetCreature(), EResourceKind::Action), 3);
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
    EXPECT_EQ(ResourceFor(*actor.GetCreature(), EResourceKind::Action)->Count(), 2);
    suspended.reset();
    EXPECT_TRUE(weak_action.expired());
}

TEST(AbilityExecutionTest, RepeatedSuspensionRevalidatesSourceBeforeInnerResumeCallback)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    transformator.AddResource(ResourceFor(*actor.GetCreature(), EResourceKind::Action), 3);
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
    EXPECT_EQ(ResourceFor(*actor.GetCreature(), EResourceKind::Action)->Count(), 2);
}

TEST(AbilityHookTest, ReplayingCompletedSavepointRevalidatesFeatureBeforeHookCallback)
{
    int resumed = 0;
    int applied = 0;
    std::shared_ptr<TActionContext> context;
    auto fixture = MakeHookBattle([&](std::shared_ptr<TActionContext> ctx) {
        context = ctx;
        throw TSavepointStackUnwind(ctx->transformator->CurrentState(), [ctx, &resumed]() {
            auto& actor = ctx->game_object_registry->Get<TPlayer>(
                TGameObjectIdManager::Instance().Register("self"));
            ctx->transformator->DealDamage(&actor, 1);
            ++resumed;
        });
    }, [&](auto) { ++applied; });
    auto& actor = Actor(*fixture);
    std::optional<TSavepointStackUnwind> suspended;
    try {
        fixture->battle->Execute(fixture->battle->AvailableActions(actor).at(0));
        FAIL() << "hook did not suspend";
    } catch (const TSavepointStackUnwind& save_point) {
        suspended = save_point;
    }
    ASSERT_TRUE(suspended.has_value());
    ASSERT_NE(context, nullptr);
    EXPECT_EQ(applied, 0);
    EXPECT_EQ(ResourceFor(*actor.GetCreature(), EResourceKind::Action)->Count(), 2);

    suspended->Resume();
    EXPECT_EQ(resumed, 1);
    EXPECT_EQ(applied, 1);
    EXPECT_EQ(actor.GetCreature()->Hitpoints()->Current(), 9);
    suspended->Revert(*context->transformator);
    EXPECT_EQ(actor.GetCreature()->Hitpoints()->Current(), 10);
    const auto before_removal = context->transformator->CurrentState();
    RemoveHookFeature(*actor.GetCreature(), *context->transformator);
    EXPECT_NO_THROW(ValidateAbilitySource(context));
    EXPECT_THROW(suspended->Resume(), TAbilityUnavailable);
    EXPECT_EQ(resumed, 1);
    EXPECT_EQ(applied, 1);
    EXPECT_EQ(actor.GetCreature()->Hitpoints()->Current(), 10);
    EXPECT_EQ(ResourceFor(*actor.GetCreature(), EResourceKind::Action)->Count(), 2);

    context->transformator->Undo(before_removal);
    suspended->Resume();
    EXPECT_EQ(resumed, 2);
    EXPECT_EQ(applied, 2);
    EXPECT_EQ(actor.GetCreature()->Hitpoints()->Current(), 9);
    EXPECT_EQ(ResourceFor(*actor.GetCreature(), EResourceKind::Action)->Count(), 2);
}

TEST(AbilityHookTest, NestedSuspensionKeepsHookValidationAndRestoresActionScope)
{
    int first_resumed = 0;
    int second_resumed = 0;
    int applied = 0;
    std::shared_ptr<TActionContext> context;
    auto fixture = MakeHookBattle([&](auto ctx) {
        context = ctx;
        throw TSavepointStackUnwind(ctx->transformator->CurrentState(), [&, ctx]() {
            ++first_resumed;
            throw TSavepointStackUnwind(ctx->transformator->CurrentState(), [&]() {
                ++second_resumed;
            });
        });
    }, [&](auto) { ++applied; });
    auto& actor = Actor(*fixture);
    std::optional<TSavepointStackUnwind> first;
    try {
        fixture->battle->Execute(fixture->battle->AvailableActions(actor).at(0));
        FAIL() << "hook did not suspend";
    } catch (const TSavepointStackUnwind& save_point) {
        first = save_point;
    }
    ASSERT_TRUE(first.has_value());
    std::optional<TSavepointStackUnwind> second;
    try {
        first->Resume();
        FAIL() << "hook did not suspend again";
    } catch (const TSavepointStackUnwind& save_point) {
        second = save_point;
    }
    ASSERT_TRUE(second.has_value());
    ASSERT_NE(context, nullptr);
    EXPECT_EQ(first_resumed, 1);
    const auto before_removal = context->transformator->CurrentState();
    RemoveHookFeature(*actor.GetCreature(), *context->transformator);
    EXPECT_THROW(second->Resume(), TAbilityUnavailable);
    EXPECT_EQ(second_resumed, 0);
    EXPECT_EQ(applied, 0);
    context->transformator->Undo(before_removal);
    second->Resume();
    EXPECT_EQ(first_resumed, 1);
    EXPECT_EQ(second_resumed, 1);
    EXPECT_EQ(applied, 1);
    EXPECT_EQ(ResourceFor(*actor.GetCreature(), EResourceKind::Action)->Count(), 2);
    RemoveHookFeature(*actor.GetCreature(), *context->transformator);
    EXPECT_NO_THROW(ValidateAbilitySource(context));
}

TEST(AbilityBattleTest, InvalidAndDuplicateIdentitiesDoNotChangeBattle)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    auto extra = fixture->factory.Create<TCreature>(
        TGameObjectIdManager::Instance().Register("warrior"));
    auto restored = TCreature::Restore(actor.GetCreature()->ToJson(), fixture->factory);
    auto moved_from = fixture->factory.Create<TCreature>(
        TGameObjectIdManager::Instance().Register("warrior"));
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
    EXPECT_THROW(add(&restored, 2), std::invalid_argument);
    EXPECT_THROW(add(&moved_from, 2), std::invalid_argument);
    EXPECT_NE(moved.Id().Value, 0);
    TAstContext after_context;
    EXPECT_EQ(fixture->battle->GetAst(after_context), before);
    EXPECT_EQ(fixture->rng.RemainingCalls(), 0);
}

TEST(AbilityBattleTest, FailedInitiativeRestoresMapAndPlayers)
{
    auto fixture = MakeTwoWarriorBattle();
    auto extra = fixture->factory.Create<TCreature>(
        TGameObjectIdManager::Instance().Register("warrior"));
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
    transformator.AddResource(actor.GetCreature()->Hands(), 5);
    const auto before = transformator.CurrentState();
    const auto cost = BindResource("self.hands", 1, &TCreature::Hands, &TCreature::Hands);
    auto alias = cost;
    alias.Key = "alias.hands";
    int applied = 0;
    auto action = std::make_shared<TAction>([&](std::shared_ptr<TActionContext> ctx) {
        ++applied;
        EXPECT_EQ(&ctx->game_object_registry->Get<TPlayer>(
            TGameObjectIdManager::Instance().Register("self")), &actor);
        EXPECT_EQ(ctx->next_block, nullptr);
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
    transformator.AddResource(actor.GetCreature()->Hands(), 3);
    transformator.AddResource(actor.GetCreature()->Actions(), 3);
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
    transformator.AddResource(actor.GetCreature()->Hands(), 3);
    transformator.AddResource(actor.GetCreature()->Actions(), 3);
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
    transformator.ReduceResource(actor.GetCreature()->Actions(), 2);
    EXPECT_THROW(valid.Consume(MakeContext(*fixture, transformator), actor), TAbilityUnavailable);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 1);
}

TEST(GeneratedActionTest, PaidCostsComposeWithOuterRollbackAndBodyFailures)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    transformator.AddResource(actor.GetCreature()->Hands(), 3);
    transformator.AddResource(actor.GetCreature()->Actions(), 3);
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

TEST(GeneratedActionTest, SuspendedBodyRevalidatesWithoutRepeatingPayment)
{
    auto fixture = MakeTwoWarriorBattle();
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    transformator.AddResource(actor.GetCreature()->Hands(), 3);
    const auto catalog = MakeCatalog();
    TAbilityCollector collector(catalog);
    collector.Grant("test.Strike", {}, "self");
    int entered = 0;
    int first_resumed = 0;
    int second_resumed = 0;
    bool present = true;
    auto action = std::make_shared<TAction>([&](auto ctx) {
        ++entered;
        throw TSavepointStackUnwind(ctx->transformator->CurrentState(), [&, ctx]() {
            ++first_resumed;
            throw TSavepointStackUnwind(ctx->transformator->CurrentState(), [&]() {
                ++second_resumed;
            });
        });
    }, TAction::TBoundResources{
        BindResource("self.hands", 2, &TCreature::Hands, &TCreature::Hands),
    }, "suspended body");
    TAbilityExecutions executions;
    executions.Register("test.Strike", action);
    const auto option = EnumerateActions(actor.GetId(), collector, executions, actor).at(0);
    std::optional<TSavepointStackUnwind> first;
    try {
        ExecuteAbility(option, MakeContext(*fixture, transformator), actor, collector, executions,
                       {.ValidateSource = [&present]() { return present; }});
        FAIL() << "generated body did not suspend";
    } catch (const TSavepointStackUnwind& save_point) {
        first = save_point;
    }
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(entered, 1);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 1);
    first->Revert(transformator);
    std::optional<TSavepointStackUnwind> second;
    try {
        first->Resume();
        FAIL() << "generated body did not suspend again";
    } catch (const TSavepointStackUnwind& save_point) {
        second = save_point;
    }
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first_resumed, 1);
    present = false;
    EXPECT_THROW(second->Resume(), TAbilityUnavailable);
    EXPECT_EQ(second_resumed, 0);
    present = true;
    second->Resume();
    EXPECT_EQ(entered, 1);
    EXPECT_EQ(first_resumed, 1);
    EXPECT_EQ(second_resumed, 1);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 1);
}

TEST(GeneratedActionTest, DirectBodyDoesNotEnterLegacyBlockHooks)
{
    int hooks = 0;
    int legacy_actions = 0;
    int bodies = 0;
    auto fixture = MakeHookBattle([&](auto) { ++hooks; }, [&](auto) { ++legacy_actions; });
    auto& actor = Actor(*fixture);
    TTransformator transformator(fixture->io);
    const TAction generated([&](auto) { ++bodies; }, {}, "probe");
    generated.Apply(MakeContext(*fixture, transformator), actor);
    EXPECT_EQ(bodies, 1);
    EXPECT_EQ(hooks, 0);
    EXPECT_EQ(legacy_actions, 0);
}

#include <pf2e_engine/actions/feature_event.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

#include "../test_lib/mock_dice_roller.h"
#include "../test_lib/mock_interaction_system.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace {

struct TRegistryEvent {
    using FHandler = std::function<int()>;
    static constexpr std::string_view kId = "test.RegistryEvent";
};

struct TRegistryConflict {
    using FHandler = std::function<bool()>;
    static constexpr std::string_view kId = "test.RegistryEvent";
};

struct TTestBonus {
    using TOwner = TCreature;
    using TResult = int;
    using TArguments = std::tuple<TGeneratedEntityBinding<TCreature>, int>;
    using FHandler = std::function<TResult(const TGeneratedEntityBinding<TOwner>&,
                                            const TArguments&)>;
    static constexpr std::string_view kId = "test.Bonus";
};

struct TTestOtherBonus : TTestBonus {
    static constexpr std::string_view kId = "test.OtherBonus";
};

struct TTestValueBonus {
    using TOwner = TCreature;
    using TResult = Statistic;
    using TArguments = std::tuple<>;
    using FHandler = std::function<TResult(const TGeneratedEntityBinding<TOwner>&,
                                            const TArguments&)>;
    static constexpr std::string_view kId = "test.ValueBonus";
};

TIdCollection<TFeatureSelection> Selection(std::string feature)
{
    TIdCollection<TFeatureSelection> result;
    if (!feature.empty()) {
        result.Add(TFeatureSelection(std::move(feature)));
    }
    return result;
}

std::shared_ptr<const TAbilityRules> MakeRules(TTestBonus::FHandler handler)
{
    auto rules = std::make_shared<TAbilityRules>();
    rules->Catalog.Register({"test.Helpful", EAbilityKind::Feature, {}});
    rules->Catalog.Register({"test.Left", EAbilityKind::Feature, {"test.Helpful"}});
    rules->Catalog.Register({"test.Right", EAbilityKind::Feature, {"test.Helpful"}});
    rules->Catalog.Register({"test.Diamond", EAbilityKind::Feature, {"test.Left", "test.Right"}});
    rules->Events.Register<TTestBonus>("test.Helpful",
        std::make_shared<const TTestBonus::FHandler>(std::move(handler)));
    rules->Events.Register<TTestOtherBonus>("test.Helpful",
        std::make_shared<const TTestOtherBonus::FHandler>([](const auto&, const auto&) { return 5; }));
    rules->Events.Register<TTestValueBonus>("test.Helpful",
        std::make_shared<const TTestValueBonus::FHandler>([](const auto& owner, const auto&) {
            return Statistic::Create({.Base = owner.Read().Level()});
        }));
    return rules;
}

class TFeatureEventRuntimeTest : public ::testing::Test {
protected:
    void Start(std::shared_ptr<const TAbilityRules> rules)
    {
        rules_ = std::move(rules);
        nlohmann::json map = {{"x_size", 8}, {"y_size", 8}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, rules_);
        transformator_ = std::make_unique<TTransformator>(io_);
    }

    TPlayer& Add(std::string feature = {}, std::string class_feature = {},
                 int level = 1, int current = 10)
    {
        const int id = static_cast<int>(creatures_.size());
        creatures_.push_back(TCreature::Create({
            .Level = level,
            .Class = TClass::Create({.Features = Selection(std::move(class_feature))}),
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 10}), .Current = current,
            }),
            .Features = Selection(std::move(feature)),
        }));
        rng_.ExpectCall(20, 10);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id}, TPlayerId{id},
                                   "Player " + std::to_string(id), ""), {id, 0});
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    std::shared_ptr<TActionContext> Context(TPlayer& actor)
    {
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(),
            .dice_roller = &rng_,
            .transformator = transformator_.get(),
            .io_system = &io_,
            .scheduler = nullptr,
            .effect_manager = nullptr,
        });
        context->game_object_registry = std::make_shared<TGameObjectRegistry>();
        context->game_object_registry->Add(
            TGameObjectIdManager::Instance().Register("self"), &actor);
        return context;
    }

    void TearDown() override
    {
        rng_.Verify();
    }

    TMockRng rng_;
    TMockInteractionSystem io_;
    std::deque<TCreature> creatures_;
    std::shared_ptr<const TAbilityRules> rules_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> transformator_;
};

TEST(FeatureEventRegistryTest, TypedHandlersAreImmutableAndRegistrationIsIdempotent)
{
    TFeatureEventRegistry registry;
    const auto handler = std::make_shared<const TRegistryEvent::FHandler>([]() { return 3; });
    registry.Register<TRegistryEvent>("test.Feature", handler);
    registry.Register<TRegistryEvent>("test.Feature", handler);
    EXPECT_EQ(registry.Find<TRegistryEvent>("test.Feature"), handler);
    EXPECT_EQ((*registry.Find<TRegistryEvent>("test.Feature"))(), 3);
    EXPECT_EQ(registry.Find<TRegistryEvent>("missing"), nullptr);
    EXPECT_THROW(registry.Register<TRegistryEvent>("test.Feature",
        std::make_shared<const TRegistryEvent::FHandler>([]() { return 3; })), std::invalid_argument);
    EXPECT_THROW(registry.Register<TRegistryEvent>("", handler), std::invalid_argument);
    EXPECT_THROW(registry.Register<TRegistryEvent>("null", nullptr), std::invalid_argument);
    EXPECT_THROW(registry.Register<TRegistryEvent>("empty",
        std::make_shared<const TRegistryEvent::FHandler>()), std::invalid_argument);
    EXPECT_THROW(registry.Register<TRegistryConflict>("different",
        std::make_shared<const TRegistryConflict::FHandler>([]() { return true; })),
        std::invalid_argument);
    EXPECT_THROW(registry.Find<TRegistryConflict>("test.Feature"), std::invalid_argument);
    EXPECT_EQ(registry.Find<TRegistryEvent>("different"), nullptr);
    const auto copied = registry;
    registry = {};
    EXPECT_EQ((*copied.Find<TRegistryEvent>("test.Feature"))(), 3);
}

TEST_F(TFeatureEventRuntimeTest, HandlersReceiveReadonlyOwnerAndArgumentsWithStableIdentity)
{
    int calls = 0;
    Start(MakeRules([&](const auto& owner, const auto& arguments) {
        ++calls;
        const auto& target = std::get<0>(arguments);
        EXPECT_THROW(owner.Guard(), std::logic_error);
        EXPECT_THROW(target.Guard(), std::logic_error);
        EXPECT_THROW(owner.View().MutablePlayer(), std::logic_error);
        EXPECT_EQ(owner, BindActionEntity<TCreature>(owner.View().Player()));
        EXPECT_EQ(target, BindActionEntity<TCreature>(target.View().Player()));
        return owner.Read().Level() + target.Read().Level() + std::get<1>(arguments);
    }));
    auto& actor = Add("test.Helpful", {}, 2);
    auto& target = Add({}, {}, 3);
    TAstContext before_context;
    const auto before = battle_->GetAst(before_context);
    const auto actor_binding = BindActionEntity<TCreature>(actor);
    const auto target_binding = BindActionEntity<TCreature>(target);
    const auto result = DispatchFeatureEvent<TTestBonus>(Context(actor), actor_binding,
                                                        {target_binding, 7});
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].Value, 12);
    EXPECT_EQ(result[0].OwnerId, actor.GetCreature()->Id());
    EXPECT_EQ(calls, 1);
    EXPECT_NO_THROW(actor_binding.Guard());
    EXPECT_NO_THROW(target_binding.Guard());
    EXPECT_EQ(actor_binding.ReadOnly(), actor_binding);
    TAstContext after_context;
    EXPECT_EQ(battle_->GetAst(after_context), before);
}

TEST_F(TFeatureEventRuntimeTest, CurrentOwnerSourcesDeduplicateDiamondsAndPreserveSnapshots)
{
    Start(MakeRules([](const auto&, const auto&) { return 2; }));
    auto& actor = Add("test.Diamond", "test.Helpful");
    auto& target = Add("test.Helpful");
    auto& unrelated = Add();
    const auto owner = BindActionEntity<TCreature>(actor);
    const TTestBonus::TArguments arguments{BindActionEntity<TCreature>(target), 1};
    const auto snapshot = DispatchFeatureEvent<TTestBonus>(*rules_, owner, arguments);
    ASSERT_EQ(snapshot.size(), 2);
    EXPECT_EQ(snapshot[0].FeatureId, "test.Helpful");
    EXPECT_EQ(snapshot[0].Source, TAbilitySourcePath{}.Field("features").Entry(0));
    EXPECT_EQ(snapshot[0].Provenance.size(), 2);
    EXPECT_EQ(snapshot[1].Source,
              TAbilitySourcePath{}.Field("class").Field("features").Entry(0));
    EXPECT_EQ(snapshot[1].Provenance.size(), 1);
    EXPECT_TRUE(DispatchFeatureEvent<TTestBonus>(*rules_,
        BindActionEntity<TCreature>(unrelated), arguments).empty());

    const auto before = transformator_->CurrentState();
    auto execution = transformator_->ExecutionContext();
    execution.AssignGuarded(actor.GetCreature()->Class(), TClass::Create({}),
                            TGuarded<TCreature>(*actor.GetCreature()), "class");
    const auto current = DispatchFeatureEvent<TTestBonus>(*rules_, owner, arguments);
    ASSERT_EQ(current.size(), 1);
    EXPECT_EQ(current[0].Source, snapshot[0].Source);
    EXPECT_EQ(snapshot.size(), 2);
    EXPECT_EQ(snapshot[1].Value, 2);
    transformator_->Undo(before);
    EXPECT_EQ(DispatchFeatureEvent<TTestBonus>(*rules_, owner, arguments).size(), 2);
}

TEST_F(TFeatureEventRuntimeTest, OrdinaryClassContributionsAreIndependentValueSnapshots)
{
    Start(MakeRules([](const auto&, const auto&) { return 2; }));
    auto& actor = Add("test.Helpful", {}, 2);
    const auto owner = BindActionEntity<TCreature>(actor);
    const auto result = DispatchFeatureEvent<TTestValueBonus>(*rules_, owner, {});
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].Value.Base(), 2);
    auto execution = transformator_->ExecutionContext();
    execution.AssignGuarded(actor.GetCreature()->Level(), 4,
                            TGuarded<TCreature>(*actor.GetCreature()), "level");
    const auto current = DispatchFeatureEvent<TTestValueBonus>(*rules_, owner, {});
    ASSERT_EQ(current.size(), 1);
    EXPECT_EQ(current[0].Value.Base(), 4);
    EXPECT_EQ(result[0].Value.Base(), 2);
}

TEST_F(TFeatureEventRuntimeTest, ContextRejectsForeignArgumentsBeforeCallingHandlers)
{
    int calls = 0;
    Start(MakeRules([&](const auto&, const auto&) { ++calls; return 2; }));
    auto& actor = Add("test.Helpful");
    auto foreign_creature = TCreature::Create({.Level = 1});
    TPlayer foreign(&foreign_creature, TPlayerTeam{0}, TPlayerId{0}, "Foreign", "");
    const auto foreign_binding = BindActionEntity<TCreature>(foreign);
    EXPECT_THROW(DispatchFeatureEvent<TTestBonus>(Context(actor),
        BindActionEntity<TCreature>(actor), {foreign_binding, 1}), TAbilityUnavailable);
    EXPECT_EQ(calls, 0);
    const auto result = DispatchFeatureEvent<TTestBonus>(*rules_,
        BindActionEntity<TCreature>(actor), {foreign_binding, 1});
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(calls, 1);
}

TEST_F(TFeatureEventRuntimeTest, ReadonlyDispatchAllowsDefeatedOwnerAndRejectsStaleBindings)
{
    int calls = 0;
    Start(MakeRules([&](const auto&, const auto&) { ++calls; return 2; }));
    auto& actor = Add("test.Helpful", {}, 1, 0);
    auto target_creature = TCreature::Create({.Level = 1});
    TPlayer target(&target_creature, TPlayerTeam{1}, TPlayerId{1}, "Detached", "");
    const auto target_binding = BindActionEntity<TCreature>(target);
    const auto owner = BindActionEntity<TCreature>(actor);
    EXPECT_EQ(DispatchFeatureEvent<TTestBonus>(*rules_, owner, {target_binding, 1}).size(), 1);
    EXPECT_EQ(calls, 1);
    target_creature = TCreature::Create({.Level = 2});
    EXPECT_THROW(DispatchFeatureEvent<TTestBonus>(*rules_, owner, {target_binding, 1}),
                 TAbilityUnavailable);
    EXPECT_EQ(calls, 1);
}

TEST_F(TFeatureEventRuntimeTest, NestedDispatchFailsAndRestoresTheDispatchScope)
{
    Start(MakeRules([&](const auto& owner, const auto& arguments) {
        return DispatchFeatureEvent<TTestOtherBonus>(*rules_, owner, arguments).at(0).Value;
    }));
    auto& actor = Add("test.Helpful");
    const auto owner = BindActionEntity<TCreature>(actor);
    const TTestBonus::TArguments arguments{owner, 1};
    EXPECT_THROW(DispatchFeatureEvent<TTestBonus>(*rules_, owner, arguments), std::logic_error);
    const auto recovered = DispatchFeatureEvent<TTestOtherBonus>(*rules_, owner, arguments);
    ASSERT_EQ(recovered.size(), 1);
    EXPECT_EQ(recovered[0].Value, 5);
}

TEST_F(TFeatureEventRuntimeTest, SuspendingHandlerIsRejectedAndLaterDispatchStillWorks)
{
    Start(MakeRules([&](const auto&, const auto&) -> int {
        throw TSavepointStackUnwind(transformator_->CurrentState(), []() {});
    }));
    auto& actor = Add("test.Helpful");
    const auto owner = BindActionEntity<TCreature>(actor);
    const TTestBonus::TArguments arguments{owner, 1};
    EXPECT_THROW(DispatchFeatureEvent<TTestBonus>(*rules_, owner, arguments), std::logic_error);
    const auto recovered = DispatchFeatureEvent<TTestOtherBonus>(*rules_, owner, arguments);
    ASSERT_EQ(recovered.size(), 1);
    EXPECT_EQ(recovered[0].Value, 5);
}

}

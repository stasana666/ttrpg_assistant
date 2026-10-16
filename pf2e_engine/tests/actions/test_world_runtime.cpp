#include <pf2e_engine/actions/feature_event.h>
#include <pf2e_engine/actions/generated_world.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

#include "../test_lib/mock_dice_roller.h"
#include "../test_lib/mock_interaction_system.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>

namespace {

class TIndexChoiceIo final : public IInteractionSystem {
public:
    enum class EMode { Immediate, Deferred, Failed, Missing, Twice };
    EMode Mode = EMode::Immediate;
    std::size_t Index = 0;
    std::function<void(std::size_t)> Receiver;
    std::function<void()> BeforeReturn;

    void RequestChoice(int, TAlternatives, TState state,
                       std::function<void(std::size_t)> receive) override
    {
        Receiver = std::move(receive);
        if (Mode == EMode::Deferred) {
            throw TSavepointStackUnwind(state, [saved = Receiver, index = Index] { saved(index); });
        }
        if (Mode == EMode::Failed) {
            throw std::runtime_error("failed choice request");
        }
        if (Mode != EMode::Missing) {
            Receiver(Index);
            if (Mode == EMode::Twice) {
                Receiver(Index);
            }
        }
        if (BeforeReturn) {
            BeforeReturn();
        }
    }

    void HandleReactionTrigger(const TTriggerContext&, const TState&) override {}
    std::ostream& GameLog() override { return output_; }
    std::ostream& DevLog() override { return output_; }

protected:
    std::size_t ChooseAlternativeIndex(int, const TAlternatives&) override { return Index; }

private:
    std::ostringstream output_;
};

struct TWorldBonus {
    using TOwner = TCreature;
    using TResult = int;
    using TArguments = std::tuple<TGeneratedEntityBinding<TCreature>>;
    using FHandler = std::function<int(const TGeneratedWorldView&,
        const TGeneratedEntityBinding<TCreature>&, const TArguments&)>;
    static constexpr std::string_view kId = "world.Bonus";
};

TAction::TBoundResources ProbeCosts()
{
    return {{
        .Key = "hands", .Count = 1,
        .Read = [](const TPlayer& player) -> const TResource& {
            return player.GetCreature()->Hands();
        },
        .Write = [](TPlayer& player) { return player.GetCreature()->Hands(); },
    }};
}

bool HasNearbyTarget(const TGeneratedWorldView& world, const TPlayer& actor,
                      const TAbilitySourcePath&)
{
    const auto self = world.Bind<TCreature>(actor);
    for (const auto& target : world.Entities<TCreature>()) {
        if (target != self && target.Read().IsAlive()
            && world.GridDistance(self.View(), target.View()) <= 1) {
            return true;
        }
    }
    return false;
}

class TWorldRuntimeTest : public ::testing::Test {
protected:
    void Start(FAbilityRequirement requirement = {}, TAction::FBody body = [](auto) {},
                TWorldBonus::FHandler handler = {})
    {
        rules_ = std::make_shared<TAbilityRules>();
        rules_->Catalog.Register({"world.Probe", EAbilityKind::Action, {}});
        rules_->Catalog.Register({"world.Selected", EAbilityKind::Feature, {"world.Probe"}});
        rules_->Executions.Register("world.Probe",
            std::make_shared<const TAbilityExecution>(TAbilityExecution{
                .Action = std::make_shared<const TAction>(std::move(body), ProbeCosts(), "probe"),
                .Requirement = std::move(requirement),
            }));
        if (handler) {
            rules_->Events.Register<TWorldBonus>("world.Selected",
                std::make_shared<const TWorldBonus::FHandler>(std::move(handler)));
        }
        nlohmann::json map = {{"x_size", 8}, {"y_size", 8}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, rules_);
        journal_ = std::make_unique<TTransformator>(io_);
    }

    TPlayer& Add(TPosition position, int team = 0, int current = 10, bool selected = true)
    {
        const int id = static_cast<int>(creatures_.size());
        TIdCollection<TFeatureSelection> features;
        if (selected) {
            features.Add(TFeatureSelection("world.Selected"));
        }
        creatures_.push_back(TCreature::Create({
            .Level = 1,
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 10}), .Current = current,
            }),
            .Features = std::move(features), .Hands = TResource(3),
        }));
        rng_.ExpectCall(20, 10);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{team}, TPlayerId{id},
                                   "Actor " + std::to_string(id), ""), position);
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    std::shared_ptr<TActionContext> Context(TPlayer* self = nullptr)
    {
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = journal_.get(),
            .io_system = &io_, .scheduler = nullptr, .effect_manager = nullptr,
        });
        if (self != nullptr) {
            context->game_object_registry = std::make_shared<TGameObjectRegistry>();
            context->game_object_registry->Add(
                TGameObjectIdManager::Instance().Register("self"), self);
        }
        return context;
    }

    TAbilityCollector Collect(const TPlayer& actor) const
    {
        return CollectAbilities(*actor.GetCreature(), rules_->Catalog);
    }

    TAbilityOption BoundOption(const TPlayer& actor) const
    {
        const auto collector = Collect(actor);
        const auto& grant = collector.Grants().at(0);
        return {
            .ActorId = actor.GetId(), .ActorIdentity = &actor,
            .DefinitionId = grant.DefinitionId, .Source = grant.Source,
            .Provenance = grant.Provenance,
            .Action = rules_->Executions.Require(grant.DefinitionId).Action,
        };
    }

    void TearDown() override
    {
        rng_.Verify();
    }

    TMockRng rng_;
    TMockInteractionSystem io_;
    std::deque<TCreature> creatures_;
    std::shared_ptr<TAbilityRules> rules_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> journal_;
};

}

TEST_F(TWorldRuntimeTest, ValueChoiceReturnsSynchronouslyWithoutCallingContinuation)
{
    Start();
    auto& actor = Add({1, 1});
    auto context = Context(&actor);
    TIndexChoiceIo interaction;
    context->io_system = &interaction;
    int resumed = 0;
    const auto value = SelectActionValue<int>(context, {3, 7},
        [](const int& candidate) { return candidate > 4; },
        [&](int) { ++resumed; });
    EXPECT_EQ(value, 7);
    EXPECT_EQ(resumed, 0);
    EXPECT_THROW(interaction.Receiver(0), std::logic_error);
    EXPECT_EQ(resumed, 0);
    interaction.Receiver = {};
    std::weak_ptr<TActionContext> weak = context;
    context.reset();
    EXPECT_TRUE(weak.expired());
}

TEST_F(TWorldRuntimeTest, DeferredValueChoiceRechecksPredicateAndCanReplayItsSavedContinuation)
{
    Start();
    auto& actor = Add({1, 1});
    auto context = Context(&actor);
    TIndexChoiceIo interaction;
    interaction.Mode = TIndexChoiceIo::EMode::Deferred;
    context->io_system = &interaction;
    bool eligible = true;
    int resumed = 0;
    std::optional<TSavepointStackUnwind> point;
    try {
        SelectActionValue<int>(context, {7}, [&](const int&) { return eligible; },
            [&](int value) { EXPECT_EQ(value, 7); ++resumed; });
        FAIL() << "expected suspended choice";
    } catch (const TSavepointStackUnwind& saved) {
        point = saved;
    }
    ASSERT_TRUE(point);
    EXPECT_EQ(resumed, 0);
    eligible = false;
    EXPECT_THROW(point->Resume(), TAbilityUnavailable);
    EXPECT_EQ(resumed, 0);
    eligible = true;
    point->Resume();
    point->Resume();
    EXPECT_EQ(resumed, 2);
}

TEST_F(TWorldRuntimeTest, ExplicitEntityChoiceCanSelectReadonlyWorldCandidatesWithoutChangingAliases)
{
    Start();
    auto& actor = Add({1, 1});
    auto& target = Add({2, 1});
    auto context = Context(&actor);
    TIndexChoiceIo interaction;
    context->io_system = &interaction;
    const auto candidates = TGeneratedWorldView(battle_.get()).Entities<TCreature>();
    const auto chosen = SelectActionEntity<TCreature>(context, candidates,
        [&target](const auto& candidate) { return candidate.Read().Id() == target.GetCreature()->Id(); },
        [](auto) { FAIL() << "synchronous choice called continuation"; });
    EXPECT_EQ(&chosen.Guard().Get(), target.GetCreature());
    for (const auto& candidate : candidates) {
        EXPECT_THROW(candidate.Guard(), std::logic_error);
    }
    auto execution = journal_->ExecutionContext();
    const auto state = execution.GetState();
    execution.Invoke(chosen.Guard(), &TCreature::ReceiveDamage, EDamageType::Fire, 3);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 7);
    execution.Undo(state);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 10);
}

TEST_F(TWorldRuntimeTest, ChoiceRetainsOriginalActorAcrossInlineReturnAndDeferredReplay)
{
    Start();
    auto& actor = Add({1, 1});
    auto& other = Add({2, 1});
    auto context = Context(&actor);
    const auto original = context->game_object_registry;
    const auto replacement = Context(&other)->game_object_registry;
    TIndexChoiceIo interaction;
    context->io_system = &interaction;
    int resumed = 0;
    interaction.BeforeReturn = [&] { context->game_object_registry = replacement; };
    EXPECT_THROW(SelectActionValue<int>(context, {1}, {}, [&](int) { ++resumed; }), TAbilityUnavailable);
    context->game_object_registry = original;
    EXPECT_THROW(interaction.Receiver(0), std::logic_error);
    interaction.BeforeReturn = {};
    interaction.Mode = TIndexChoiceIo::EMode::Deferred;
    std::optional<TSavepointStackUnwind> point;
    try {
        SelectActionValue<int>(context, {1}, {}, [&](int) { ++resumed; });
        FAIL() << "expected suspended choice";
    } catch (const TSavepointStackUnwind& saved) {
        point = saved;
    }
    ASSERT_TRUE(point);
    context->game_object_registry = replacement;
    EXPECT_THROW(point->Resume(), TAbilityUnavailable);
    EXPECT_EQ(resumed, 0);
    context->game_object_registry = original;
    point->Resume();
    EXPECT_EQ(resumed, 1);
}

TEST_F(TWorldRuntimeTest, FailedMissingDuplicateAndInvalidChoicesCannotResume)
{
    Start();
    auto& actor = Add({1, 1});
    auto context = Context(&actor);
    TIndexChoiceIo interaction;
    context->io_system = &interaction;
    int resumed = 0;
    for (const auto mode : {TIndexChoiceIo::EMode::Failed, TIndexChoiceIo::EMode::Missing,
                           TIndexChoiceIo::EMode::Twice}) {
        interaction.Mode = mode;
        EXPECT_THROW(SelectActionValue<int>(context, {1}, {}, [&](int) { ++resumed; }), std::exception);
        EXPECT_THROW(interaction.Receiver(0), std::logic_error);
    }
    interaction.Mode = TIndexChoiceIo::EMode::Immediate;
    interaction.Index = 2;
    EXPECT_THROW(SelectActionValue<int>(context, {1}, {}, [&](int) { ++resumed; }), std::out_of_range);
    EXPECT_THROW(interaction.Receiver(0), std::logic_error);
    EXPECT_EQ(resumed, 0);
}

TEST_F(TWorldRuntimeTest, ReadsArePureAndEnumeratedBindingsAreReadonlyWithoutAliveFiltering)
{
    Start();
    auto& actor = Add({1, 1}, 4);
    auto& ally = Add({2, 1}, 4);
    auto& dead = Add({4, 5}, 8, 0);
    const TGeneratedWorldView world(battle_.get());
    TAstContext before_context;
    const auto before = battle_->GetAst(before_context);
    const auto entities = world.Entities<TCreature>();
    ASSERT_EQ(entities.size(), 3);
    EXPECT_EQ(entities[0], BindActionEntity<TCreature>(actor));
    EXPECT_EQ(entities[1], BindActionEntity<TCreature>(ally));
    EXPECT_EQ(entities[2], BindActionEntity<TCreature>(dead));
    EXPECT_FALSE(entities[2].Read().IsAlive());
    for (const auto& entity : entities) {
        EXPECT_THROW(entity.Guard(), std::logic_error);
        EXPECT_THROW(entity.View().MutablePlayer(), std::logic_error);
    }
    EXPECT_EQ(world.PositionX(entities[2].View()), 4);
    EXPECT_EQ(world.PositionY(entities[2].View()), 5);
    EXPECT_EQ(world.Team(entities[0].View()), 4);
    EXPECT_TRUE(world.SameTeam(entities[0].View(), entities[1].View()));
    EXPECT_FALSE(world.SameTeam(entities[0].View(), entities[2].View()));
    EXPECT_EQ(world.GridDistance(entities[0].View(), entities[2].View()), 4);
    EXPECT_EQ(world.GridDistance(entities[2].View(), entities[0].View()), 4);
    EXPECT_EQ(world.GridDistance(entities[0].View(), entities[0].View()), 0);
    EXPECT_TRUE(world.HasLineOfEffect(entities[0].View(), entities[2].View()));
    EXPECT_TRUE(world.Entities<TWeapon>().empty());
    EXPECT_THROW(world.Bind<TWeapon>(actor), std::invalid_argument);
    TAstContext after_context;
    EXPECT_EQ(battle_->GetAst(after_context), before);
}

TEST_F(TWorldRuntimeTest, UnboundWorldAllowsReadonlyBindingAndRejectsWorldQueries)
{
    Start();
    auto& actor = Add({0, 0});
    const TGeneratedWorldView world;
    EXPECT_FALSE(world.IsBound());
    const auto binding = world.Bind<TCreature>(actor);
    EXPECT_EQ(binding, BindActionEntity<TCreature>(actor));
    EXPECT_THROW(binding.Guard(), std::logic_error);
    EXPECT_THROW(world.Entities<TCreature>(), std::invalid_argument);
    EXPECT_THROW(world.Position(binding.View()), std::invalid_argument);
    EXPECT_THROW(world.Team(binding.View()), std::invalid_argument);
    EXPECT_THROW(world.SameTeam(binding.View(), binding.View()), std::invalid_argument);
    EXPECT_THROW(world.GridDistance(binding.View(), binding.View()), std::invalid_argument);
    EXPECT_THROW(world.HasLineOfEffect(binding.View(), binding.View()), std::invalid_argument);
}

TEST_F(TWorldRuntimeTest, WorldChecksInstanceMembershipEvenWhenAnotherBattleReusesPlayerIds)
{
    Start();
    auto& actor = Add({0, 0});
    auto foreign_creature = TCreature::Create({
        .Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 10})}),
    });
    nlohmann::json map = {{"x_size", 8}, {"y_size", 8}};
    TBattle other(TBattleMap(map), &rng_, io_, rules_);
    rng_.ExpectCall(20, 10);
    other.AddPlayer(TPlayer(&foreign_creature, TPlayerTeam{0}, TPlayerId{actor.GetId()},
                            "Other actor", ""), {0, 0});
    const TGeneratedWorldView world(battle_.get());
    const TGeneratedWorldView other_world(&other);
    const auto self = world.Bind<TCreature>(actor);
    const auto foreign = other_world.Entities<TCreature>().at(0);
    EXPECT_EQ(foreign.View().Player().GetId(), actor.GetId());
    EXPECT_THROW(world.Position(foreign.View()), TAbilityUnavailable);
    EXPECT_THROW(world.SameTeam(self.View(), foreign.View()), TAbilityUnavailable);
    EXPECT_THROW(world.GridDistance(foreign.View(), self.View()), TAbilityUnavailable);
    EXPECT_THROW(world.HasLineOfEffect(self.View(), foreign.View()), TAbilityUnavailable);
    EXPECT_THROW(world.Bind<TCreature>(foreign.View().Player()), TAbilityUnavailable);
    TPlayer alias(actor.GetCreature(), TPlayerTeam{0}, TPlayerId{actor.GetId()}, "Alias", "");
    EXPECT_THROW(world.Position(BindActionEntity<TCreature>(alias).View()), TAbilityUnavailable);
}

TEST_F(TWorldRuntimeTest, PositionsAreLiveValuesAndJournalRollbackRestoresGeometry)
{
    Start();
    auto& actor = Add({0, 0});
    auto& target = Add({1, 1});
    const TGeneratedWorldView world(battle_.get());
    const auto self = world.Bind<TCreature>(actor);
    const auto target_binding = world.Bind<TCreature>(target);
    const auto original_position = world.Position(target_binding.View());
    const auto initial = journal_->CurrentState();
    EXPECT_EQ(world.GridDistance(self.View(), target_binding.View()), 1);
    journal_->MovePlayer(&target, {4, 4});
    EXPECT_EQ(world.GridDistance(self.View(), target_binding.View()), 4);
    EXPECT_EQ(original_position, (TPosition{1, 1}));
    journal_->Undo(initial);
    EXPECT_EQ(world.Position(target_binding.View()), original_position);
    EXPECT_EQ(world.GridDistance(self.View(), target_binding.View()), 1);
}

TEST_F(TWorldRuntimeTest, EntityIdentityIsRecheckedAfterReplacementAtTheSameAddress)
{
    Start();
    auto& actor = Add({0, 0});
    const TGeneratedWorldView world(battle_.get());
    const auto previous = world.Bind<TCreature>(actor);
    const auto* original_address = actor.GetCreature();
    *actor.GetCreature() = TCreature::Create({.Level = 5});
    EXPECT_EQ(actor.GetCreature(), original_address);
    EXPECT_THROW(world.Position(previous.View()), TAbilityUnavailable);
    EXPECT_THROW(previous.Read(), TAbilityUnavailable);
    EXPECT_EQ(world.Bind<TCreature>(actor).Read().Level(), 5);
}

TEST_F(TWorldRuntimeTest, ActionWorldDoesNotOwnTheContextAndRequiresTheCorrectActingParticipant)
{
    Start();
    auto& actor = Add({0, 0});
    EXPECT_THROW(GeneratedActionWorld(Context()), std::invalid_argument);
    auto context = Context(&actor);
    const std::weak_ptr<TActionContext> weak = context;
    const auto world = GeneratedActionWorld(context);
    context.reset();
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(world.Entities<TCreature>().size(), 1);
    TPlayer alias(actor.GetCreature(), TPlayerTeam{0}, TPlayerId{actor.GetId()}, "Alias", "");
    EXPECT_THROW(GeneratedActionWorld(Context(&alias)), TAbilityUnavailable);
    journal_->DealDamage(&actor, 10);
    EXPECT_THROW(GeneratedActionWorld(Context(&actor)), TAbilityUnavailable);
    EXPECT_EQ(world.Entities<TCreature>().size(), 1);
}

TEST_F(TWorldRuntimeTest, MissingTargetsAreHiddenAndExecutionRechecksBeforePayment)
{
    int bodies = 0;
    Start(HasNearbyTarget, [&](auto) { ++bodies; });
    auto& actor = Add({0, 0});
    auto& target = Add({5, 5});
    const auto option = BoundOption(actor);
    const auto initial = journal_->CurrentState();
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    EXPECT_EQ(ExecuteAbility(option, Context(), actor, Collect(actor), rules_->Executions),
               EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    journal_->MovePlayer(&target, {1, 1});
    ASSERT_EQ(battle_->AvailableActions(actor).size(), 1);
    journal_->Undo(initial);
    EXPECT_EQ(ExecuteAbility(option, Context(), actor, Collect(actor), rules_->Executions),
               EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    journal_->MovePlayer(&target, {1, 1});
    EXPECT_EQ(ExecuteAbility(option, Context(), actor, Collect(actor), rules_->Executions),
               EAbilityExecutionResult::Applied);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
    EXPECT_EQ(bodies, 1);
    journal_->Undo(initial);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
}

TEST_F(TWorldRuntimeTest, WorldRequirementsRemainPreflightConditionsAcrossPaidContinuations)
{
    int requirements = 0;
    int resumed = 0;
    Start([&](const TGeneratedWorldView& world, const TPlayer& actor, const auto&) {
        ++requirements;
        return world.Bind<TCreature>(actor).Read().Hands().Count() == 3;
    }, [&](auto ctx) {
        throw TSavepointStackUnwind(ctx->transformator->CurrentState(), [&]() { ++resumed; });
    });
    auto& actor = Add({0, 0});
    const auto option = BoundOption(actor);
    std::optional<TSavepointStackUnwind> suspended;
    try {
        ExecuteAbility(option, Context(), actor, Collect(actor), rules_->Executions);
        FAIL();
    } catch (const TSavepointStackUnwind& savepoint) {
        suspended = savepoint;
    }
    ASSERT_TRUE(suspended);
    EXPECT_EQ(requirements, 1);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
    suspended->Resume();
    suspended->Revert(*journal_);
    suspended->Resume();
    EXPECT_EQ(resumed, 2);
    EXPECT_EQ(requirements, 1);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
}

TEST_F(TWorldRuntimeTest, BoundEligibilityRejectsForeignActors)
{
    Start();
    auto& actor = Add({0, 0});
    TPlayer alias(actor.GetCreature(), TPlayerTeam{0}, TPlayerId{actor.GetId()}, "Alias", "");
    const TGeneratedWorldView world(battle_.get());
    EXPECT_THROW(EnumerateActions(alias.GetId(), Collect(alias), rules_->Executions, alias, &world),
                 TAbilityUnavailable);
    const auto option = BoundOption(alias);
    EXPECT_THROW(ExecuteAbility(option, Context(), alias, Collect(alias), rules_->Executions),
                 TAbilityUnavailable);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
}

TEST_F(TWorldRuntimeTest, LegacyRequirementAdapterPreservesWorldlessCallersAndConflictChecks)
{
    Start();
    auto& actor = Add({0, 0});
    const auto action = rules_->Executions.Require("world.Probe").Action;
    TAbilityExecutions executions;
    int calls = 0;
    const FLegacyAbilityRequirement requirement = [&](const TPlayer& current, const auto& source) {
        ++calls;
        EXPECT_EQ(&current, &actor);
        EXPECT_EQ(source, BoundOption(actor).Source);
        return true;
    };
    executions.Register("world.Probe", action, requirement);
    EXPECT_THROW(executions.Register("world.Probe", action, requirement), std::invalid_argument);
    EXPECT_EQ(EnumerateActions(actor.GetId(), Collect(actor), executions, actor).size(), 1);
    EXPECT_EQ(calls, 1);
    const TGeneratedWorldView world(battle_.get());
    EXPECT_EQ(EnumerateActions(actor.GetId(), Collect(actor), executions, actor, &world).size(), 1);
    EXPECT_EQ(calls, 2);
}

TEST_F(TWorldRuntimeTest, TypedEventWorldUsesReadonlyBindingsAndPreservesWorldlessEvaluation)
{
    int calls = 0;
    Start({}, [](auto) {}, [&](const TGeneratedWorldView& world, const auto& owner,
                               const auto& arguments) {
        ++calls;
        const auto& target = std::get<0>(arguments);
        EXPECT_THROW(owner.Guard(), std::logic_error);
        EXPECT_THROW(target.Guard(), std::logic_error);
        if (!world.IsBound()) {
            return -1;
        }
        for (const auto& other : world.Entities<TCreature>()) {
            EXPECT_THROW(other.Guard(), std::logic_error);
        }
        return world.GridDistance(owner.View(), target.View());
    });
    auto& actor = Add({0, 0});
    auto& target = Add({3, 4});
    const auto owner = BindActionEntity<TCreature>(actor);
    const TWorldBonus::TArguments arguments{BindActionEntity<TCreature>(target)};
    EXPECT_EQ(DispatchFeatureEvent<TWorldBonus>(*rules_, owner, arguments).at(0).Value, -1);
    EXPECT_EQ(DispatchFeatureEvent<TWorldBonus>(Context(&actor), owner, arguments).at(0).Value, 4);
    const TGeneratedWorldView world(battle_.get());
    EXPECT_EQ(DispatchFeatureEvent<TWorldBonus>(*rules_, world, owner, arguments).at(0).Value, 4);
    EXPECT_EQ(calls, 3);
    TPlayer alias(target.GetCreature(), TPlayerTeam{0}, TPlayerId{target.GetId()}, "Alias", "");
    EXPECT_THROW(DispatchFeatureEvent<TWorldBonus>(*rules_, world, owner,
        {BindActionEntity<TCreature>(alias)}), TAbilityUnavailable);
    EXPECT_EQ(calls, 3);
    journal_->DealDamage(&actor, 10);
    EXPECT_EQ(DispatchFeatureEvent<TWorldBonus>(*rules_, world, owner, arguments).at(0).Value, 4);
}

TEST_F(TWorldRuntimeTest, WorldDependentHandlerRejectsAnAbsentWorldAndLaterDispatchRecovers)
{
    Start({}, [](auto) {}, [](const auto& world, const auto& owner, const auto& arguments) {
        return world.GridDistance(owner.View(), std::get<0>(arguments).View());
    });
    auto& actor = Add({0, 0});
    auto& target = Add({1, 1});
    const auto owner = BindActionEntity<TCreature>(actor);
    const TWorldBonus::TArguments arguments{BindActionEntity<TCreature>(target)};
    EXPECT_THROW(DispatchFeatureEvent<TWorldBonus>(*rules_, owner, arguments), std::invalid_argument);
    EXPECT_EQ(DispatchFeatureEvent<TWorldBonus>(Context(&actor), owner, arguments).at(0).Value, 1);
}

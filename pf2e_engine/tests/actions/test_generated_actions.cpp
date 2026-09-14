#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/action_context.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/inventory/generated_actions.h>

#include "../test_lib/mock_dice_roller.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class TGeneratedActionIo final : public IInteractionSystem {
public:
    void ExpectChoice(std::string name, bool suspend = false)
    {
        expected_.push_back({std::move(name), suspend});
    }

    void RequestChoice(int player_id, TAlternatives alternatives, TState state,
                       std::function<void(size_t)> on_choice) override
    {
        if (expected_.empty()) {
            throw std::logic_error("unexpected generated action choice");
        }
        if (!expected_.front().Suspend) {
            IInteractionSystem::RequestChoice(player_id, std::move(alternatives), state,
                                               std::move(on_choice));
            return;
        }
        const auto index = ChooseAlternativeIndex(player_id, alternatives);
        throw TSavepointStackUnwind(state,
            [on_choice = std::move(on_choice), index]() { on_choice(index); });
    }

    void HandleReactionTrigger(const TTriggerContext&, const TState&) override {}

    std::ostream& GameLog() override { return log_; }
    std::ostream& DevLog() override { return log_; }

    void Verify() const { EXPECT_TRUE(expected_.empty()); }

    const std::vector<std::string>& LastCandidates() const { return candidates_; }

protected:
    size_t ChooseAlternativeIndex(int player_id, const TAlternatives& alternatives) override
    {
        EXPECT_EQ(player_id, 0);
        if (expected_.empty()) {
            throw std::logic_error("unexpected generated action alternative");
        }
        const auto expected = expected_.front();
        expected_.pop_front();
        candidates_.clear();
        std::optional<size_t> result;
        for (size_t index = 0; index < alternatives.Size(); ++index) {
            candidates_.push_back(alternatives[index].name);
            if (alternatives[index].name == expected.Name) {
                result = index;
            }
        }
        if (!result) {
            throw std::logic_error("expected target is absent: " + expected.Name);
        }
        return *result;
    }

private:
    struct TExpectedChoice {
        std::string Name;
        bool Suspend;
    };

    std::deque<TExpectedChoice> expected_;
    std::vector<std::string> candidates_;
    std::ostringstream log_;
};

TIdCollection<TFeatureSelection> Features(const std::string& name)
{
    TIdCollection<TFeatureSelection> features;
    if (!name.empty()) {
        features.Add(TFeatureSelection("generated_actions." + name));
    }
    return features;
}

class TGeneratedActionsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        RegisterGeneratedActionsRules(*rules);
        RegisterGeneratedActionsRules(*rules);
        nlohmann::json map = {{"x_size", 8}, {"y_size", 8}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, std::move(rules));
        transformator_ = std::make_unique<TTransformator>(io_);
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
    }

    TPlayer& Add(std::string name, int current, const std::string& feature = {},
                 int temporary = 0, int hands = 0, bool class_feature = false,
                 int level = 1, const std::string& additional_class_feature = {})
    {
        const int id = static_cast<int>(creatures_.size());
        creatures_.push_back(TCreature::Create({
            .Level = level,
            .Class = TClass::Create({
                .Features = Features(class_feature ? feature : additional_class_feature),
            }),
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 20}),
                .Current = current,
                .Temporary = temporary,
            }),
            .Features = Features(class_feature ? "" : feature),
            .Actions = TResource(3),
            .Hands = TResource(hands),
        }));
        rng_.ExpectCall(20, 10);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id == 0 ? 0 : 1},
                                   TPlayerId{id}, std::move(name), ""), {id, 0});
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    TAbilityOption Option(TPlayer& actor, const std::string& name) const
    {
        const auto options = battle_->AvailableActions(actor);
        const auto found = std::find_if(options.begin(), options.end(), [&](const auto& option) {
            return option.DefinitionId == "generated_actions." + name;
        });
        if (found == options.end()) {
            throw std::logic_error("missing generated action: " + name);
        }
        return *found;
    }

    EAbilityExecutionResult ExecuteJournal(TPlayer& actor, const TAbilityOption& option)
    {
        const auto& rules = battle_->AbilityRules();
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(),
            .dice_roller = &rng_,
            .transformator = transformator_.get(),
            .io_system = &io_,
            .scheduler = nullptr,
            .effect_manager = nullptr,
        });
        return ExecuteAbility(option, std::move(context), actor,
            CollectAbilities(*actor.GetCreature(), rules.Catalog), rules.Executions,
            {.ValidateSource = [this, &actor, definition = option.DefinitionId,
                                source = option.Source]() {
                const auto fresh = CollectAbilities(*actor.GetCreature(),
                                                     battle_->AbilityRules().Catalog);
                return std::ranges::any_of(fresh.Grants(), [&](const TGrantedAbility& grant) {
                    return grant.DefinitionId == definition && grant.Source == source;
                });
            }});
    }

    TSavepointStackUnwind Suspend(const std::function<void()>& work)
    {
        try {
            work();
        } catch (const TSavepointStackUnwind& point) {
            return point;
        }
        throw std::logic_error("generated action did not suspend");
    }

    TAstNode Snapshot() const
    {
        TAstContext context;
        auto node = TAstNode::MakeObject("generated_action_execution");
        AddOwnedObject(node, "journal", *transformator_, context);
        AddOwnedObject(node, "battle", *battle_, context);
        return node;
    }

    TMockRng rng_;
    TGeneratedActionIo io_;
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> transformator_;
};

TEST_F(TGeneratedActionsTest, CompiledMendUsesClassFeatureAndChosenLivingCreature)
{
    auto& actor = Add("Actor", 20, "Mending", 0, 0, true);
    auto& first = Add("First", 6);
    auto& second = Add("Second", 18);
    Add("Defeated", 0);
    const auto option = Option(actor, "Mend");
    EXPECT_EQ(option.Source, TAbilitySourcePath{}.Field("class").Field("features").Entry(0));
    EXPECT_FALSE(option.Provenance.empty());
    EXPECT_TRUE(battle_->AvailableActions(first).empty());
    const auto before = Snapshot();
    EXPECT_EQ(battle_->AvailableActions(actor).size(), 1u);
    EXPECT_EQ(Snapshot(), before);

    io_.ExpectChoice("Second");
    rng_.ExpectCall(6, 5);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);

    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 20);
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Current(), 6);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(io_.LastCandidates(), (std::vector<std::string>{"Actor", "First", "Second"}));

    io_.ExpectChoice("First");
    rng_.ExpectCall(6, 4);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Current(), 10);
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 20);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 1);
}

TEST_F(TGeneratedActionsTest, EntityBindingEqualityUsesIdentityWithoutGrantingMutableAccess)
{
    auto& actor = Add("Actor", 20);
    auto& other = Add("Other", 20);
    const auto readonly = BindActionEntity<TCreature>(std::as_const(actor));
    const auto writable = BindActionEntity<TCreature>(actor);
    const TGeneratedEntityBinding<TCreature> battle_readonly(
        BindGeneratedActionEntity(std::as_const(actor), battle_.get()));
    const TGeneratedEntityBinding<TCreature> battle_writable(
        BindGeneratedActionEntity(actor, battle_.get()));

    EXPECT_EQ(readonly, writable);
    EXPECT_EQ(readonly, battle_readonly);
    EXPECT_EQ(readonly, battle_writable);
    EXPECT_EQ(readonly.View(), battle_writable.View());
    EXPECT_EQ(&readonly.Read(), actor.GetCreature());
    EXPECT_THROW(readonly.Guard(), std::logic_error);
    EXPECT_THROW(battle_readonly.Guard(), std::logic_error);
    EXPECT_NO_THROW(writable.Guard());
    EXPECT_NO_THROW(battle_writable.Guard());
    EXPECT_NE(readonly, BindActionEntity<TCreature>(other));

    const TGameObjectFactory factory;
    auto restored = TCreature::Restore(actor.GetCreature()->ToJson(), factory);
    TPlayer restored_player(&restored, TPlayerTeam{0}, TPlayerId{actor.GetId()},
                            "Restored", "");
    EXPECT_EQ(restored.Id(), actor.GetCreature()->Id());
    EXPECT_NE(&restored, actor.GetCreature());
    EXPECT_NE(readonly, BindActionEntity<TCreature>(restored_player));
}

TEST_F(TGeneratedActionsTest, EntityChoiceRejectsParticipantFromAnotherBattleBeforeInteraction)
{
    auto& actor = Add("Actor", 20);
    auto foreign_creature = TCreature::Create({
        .Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 20})}),
    });
    nlohmann::json map = {{"x_size", 2}, {"y_size", 2}};
    TBattle foreign_battle(TBattleMap(map), &rng_, io_);
    rng_.ExpectCall(20, 10);
    foreign_battle.AddPlayer(TPlayer(&foreign_creature, TPlayerTeam{1}, TPlayerId{0},
                                    "Foreign", ""), {0, 0});
    auto& foreign_actor = *foreign_battle.GetIfPlayers([](const TPlayer*) {
        return true;
    }).at(0);
    auto context = std::make_shared<TActionContext>(TActionContext{
        .battle = battle_.get(),
        .dice_roller = &rng_,
        .transformator = transformator_.get(),
        .io_system = &io_,
        .scheduler = nullptr,
        .effect_manager = nullptr,
        .game_object_registry = std::make_shared<TGameObjectRegistry>(),
    });
    context->game_object_registry->Add(TGameObjectIdManager::Instance().Register("self"), &actor);
    const auto candidate = BindGeneratedActionEntity(foreign_actor, &foreign_battle);
    const auto before = Snapshot();
    const auto foreign_before = foreign_creature.ToJson();
    bool called = false;

    EXPECT_THROW(RequestActionEntity(context, {candidate},
        [&called](TGeneratedEntityView) { called = true; }), TAbilityUnavailable);

    EXPECT_FALSE(called);
    EXPECT_EQ(Snapshot(), before);
    EXPECT_EQ(foreign_creature.ToJson(), foreign_before);
}

TEST_F(TGeneratedActionsTest, CompiledDamageConsumesTemporaryHitpointsOnlyOnSelectedTarget)
{
    auto& actor = Add("Actor", 20, "Injuring");
    auto& first = Add("First", 10, {}, 3);
    auto& second = Add("Second", 17, {}, 5);
    io_.ExpectChoice("First");

    EXPECT_EQ(battle_->Execute(Option(actor, "Harm")), EAbilityExecutionResult::Applied);

    EXPECT_EQ(first.GetCreature()->Hitpoints()->Current(), 6);
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Temporary(), 0);
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 17);
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Temporary(), 5);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
}

TEST_F(TGeneratedActionsTest, CompiledCostUsesDeclaredHandsResourceAndRechecksAvailability)
{
    auto& actor = Add("Actor", 20, "HeldAidTraining", 0, 2);
    auto& target = Add("Target", 18, {}, 2);
    Add("Other", 20, "HeldAidTraining", 0, 1);
    const auto option = Option(actor, "HeldAid");
    io_.ExpectChoice("Target");

    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);

    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 20);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Temporary(), 4);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 0);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Temporary(), 4);
}

TEST_F(TGeneratedActionsTest, SuspendedChoiceDefersRollAndMutationAndRechecksItsPredicate)
{
    auto& actor = Add("Actor", 20, "Mending");
    auto& target = Add("Target", 5);
    Add("Other", 20);
    const auto before = Snapshot();
    const auto initial = transformator_->CurrentState();
    io_.ExpectChoice("Target", true);
    rng_.ExpectCall(6, 3);
    const auto point = Suspend([&]() { ExecuteJournal(actor, Option(actor, "Mend")); });

    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 5);
    EXPECT_EQ(rng_.RemainingCalls(), 1u);
    const auto chosen = transformator_->CurrentState();
    transformator_->DealDamage(&target, 5);
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(rng_.RemainingCalls(), 1u);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);

    transformator_->Undo(chosen);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 8);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    transformator_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedActionsTest, ReplayingSuccessiveChoicesRestoresProgramPositionAndRolledLocals)
{
    auto& actor = Add("Actor", 20, "RelayTraining");
    auto& first = Add("First", 5);
    auto& second = Add("Second", 15);
    const auto before = Snapshot();
    const auto initial = transformator_->CurrentState();
    io_.ExpectChoice("First", true);
    io_.ExpectChoice("Second", true);
    rng_.ExpectCall(6, 4);
    const auto first_point = Suspend([&]() { ExecuteJournal(actor, Option(actor, "Relay")); });
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Current(), 5);
    EXPECT_EQ(rng_.RemainingCalls(), 1u);
    const auto second_point = Suspend([&]() { first_point.Resume(); });
    const auto after_first = Snapshot();
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Current(), 9);
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 15);

    second_point.Resume();
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 11);
    second_point.Revert(*transformator_);
    EXPECT_EQ(Snapshot(), after_first);
    second_point.Resume();
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Current(), 9);
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 11);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);

    first_point.Revert(*transformator_);
    io_.ExpectChoice("Second", true);
    rng_.ExpectCall(6, 2);
    const auto replayed_second = Suspend([&]() { first_point.Resume(); });
    replayed_second.Resume();
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Current(), 7);
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 13);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);

    transformator_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedActionsTest, FailedRequirePreservesCompletedStepsUntilCallerRollsBack)
{
    auto& actor = Add("Actor", 20, "RestrictedAidTraining");
    auto& target = Add("Target", 5);
    auto& higher = Add("Higher", 12, {}, 0, 0, false, 2);
    const auto before = Snapshot();
    const auto initial = transformator_->CurrentState();
    const auto option = Option(actor, "RestrictedAid");
    io_.ExpectChoice("Target");

    EXPECT_THROW(ExecuteJournal(actor, option), std::runtime_error);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 7);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);

    transformator_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
    io_.ExpectChoice("Higher");
    EXPECT_EQ(ExecuteJournal(actor, option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(higher.GetCreature()->Hitpoints()->Current(), 5);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    transformator_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedActionsTest, SuspendedCompiledBodyRejectsRemovedFeatureAndCanResumeAfterUndo)
{
    auto& actor = Add("Actor", 20, "Mending", 0, 0, true);
    auto& target = Add("Target", 5);
    Add("Other", 20);
    io_.ExpectChoice("Target", true);
    rng_.ExpectCall(6, 4);
    const auto point = Suspend([&]() { battle_->Execute(Option(actor, "Mend")); });
    const auto initial = transformator_->CurrentState();
    auto context = transformator_->ExecutionContext();
    context.AssignGuarded(actor.GetCreature()->Class(), TClass::Create({}),
                          TGuarded<TCreature>(*actor.GetCreature()), "class");

    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 5);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(rng_.RemainingCalls(), 1u);

    transformator_->Undo(initial);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 9);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
}

TEST_F(TGeneratedActionsTest, DefeatedActorCannotStartCompiledAction)
{
    auto& actor = Add("Actor", 20, "Mending");
    Add("Target", 5);
    const auto option = Option(actor, "Mend");
    transformator_->DealDamage(&actor, 20);

    EXPECT_TRUE(battle_->AvailableActions(actor).empty());
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
}

TEST_F(TGeneratedActionsTest, DirectExecutorRejectsDefeatedActorBeforeCostOrEffects)
{
    auto& actor = Add("Actor", 20, "Mending");
    auto& target = Add("Target", 5);
    const auto option = Option(actor, "Mend");
    transformator_->DealDamage(&actor, 20);
    const auto before = Snapshot();

    EXPECT_EQ(ExecuteJournal(actor, option), EAbilityExecutionResult::Unavailable);

    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 3);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 5);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedActionsTest, SuspendedCompiledBodyRejectsDefeatedActor)
{
    auto& actor = Add("Actor", 20, "Mending");
    auto& target = Add("Target", 5);
    io_.ExpectChoice("Target", true);
    rng_.ExpectCall(6, 3);
    const auto point = Suspend([&]() { battle_->Execute(Option(actor, "Mend")); });
    const auto initial = transformator_->CurrentState();
    transformator_->DealDamage(&actor, 20);

    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 5);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
    EXPECT_EQ(rng_.RemainingCalls(), 1u);

    transformator_->Undo(initial);
    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 8);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
}

TEST_F(TGeneratedActionsTest, FeatureEventsUseOwnerSelectionsAndAllowAnEmptyResult)
{
    auto& actor = Add("Actor", 20, "AssistedMending");
    auto& target = Add("Target", 5, "Helpful");
    EXPECT_TRUE(battle_->AvailableActions(target).empty());
    io_.ExpectChoice("Target");
    rng_.ExpectCall(6, 3);

    EXPECT_EQ(battle_->Execute(Option(actor, "AssistedMend")), EAbilityExecutionResult::Applied);

    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 8);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
}

TEST_F(TGeneratedActionsTest, FeatureWithoutActionGrantsContributesThroughItsSelectedParent)
{
    auto& actor = Add("Actor", 20, "HelpfulMending");
    auto& target = Add("Target", 5);
    const auto collected = CollectAbilities(*actor.GetCreature(), battle_->AbilityRules().Catalog);
    ASSERT_EQ(collected.Grants().size(), 1u);
    EXPECT_TRUE(std::ranges::any_of(collected.Features(), [](const TGrantedAbility& feature) {
        return feature.DefinitionId == "generated_actions.Helpful";
    }));
    io_.ExpectChoice("Target");
    rng_.ExpectCall(6, 3);

    EXPECT_EQ(battle_->Execute(Option(actor, "AssistedMend")), EAbilityExecutionResult::Applied);

    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 10);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
}

TEST_F(TGeneratedActionsTest, TypedFeatureHandlerReadsOwnerTargetAndOriginalScalarArgument)
{
    auto& actor = Add("Actor", 20, "ParameterizedMending", 0, 0, false, 2);
    auto& target = Add("Target", 2, {}, 0, 0, false, 3);
    io_.ExpectChoice("Target");
    rng_.ExpectCall(6, 4);

    EXPECT_EQ(battle_->Execute(Option(actor, "AssistedMend")), EAbilityExecutionResult::Applied);

    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 17);
    EXPECT_EQ(actor.GetCreature()->Hitpoints()->Current(), 20);
}

TEST_F(TGeneratedActionsTest, EventHandlersDeduplicateGrantPathsButPreserveDistinctSources)
{
    auto& actor = Add("Actor", 20, "DiamondMending", 0, 0, false, 1, "Helpful");
    auto& target = Add("Target", 5);
    const auto collected = CollectAbilities(*actor.GetCreature(), battle_->AbilityRules().Catalog);
    EXPECT_EQ(std::ranges::count_if(collected.Features(), [](const TGrantedAbility& feature) {
        return feature.DefinitionId == "generated_actions.Helpful";
    }), 2);
    io_.ExpectChoice("Target");
    rng_.ExpectCall(6, 3);

    EXPECT_EQ(battle_->Execute(Option(actor, "AssistedMend")), EAbilityExecutionResult::Applied);

    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 12);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
}

TEST_F(TGeneratedActionsTest, EventDispatchReadsSourcesPresentWhenTheActionResumes)
{
    auto& actor = Add("Actor", 20, "AssistedMending", 0, 0, false, 1, "Helpful");
    auto& target = Add("Target", 5);
    const auto option = Option(actor, "AssistedMend");
    io_.ExpectChoice("Target", true);
    rng_.ExpectCall(6, 3);
    const auto point = Suspend([&]() { battle_->Execute(option); });
    const auto initial = transformator_->CurrentState();
    auto context = transformator_->ExecutionContext();
    context.AssignGuarded(actor.GetCreature()->Class(), TClass::Create({}),
                          TGuarded<TCreature>(*actor.GetCreature()), "class");

    point.Resume();
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 8);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);

    transformator_->Undo(initial);
    io_.ExpectChoice("Target");
    rng_.ExpectCall(6, 3);
    EXPECT_EQ(battle_->Execute(option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 13);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 1);
}

TEST_F(TGeneratedActionsTest, FoldedEventValuesSurviveSuspensionAndReevaluateOnEarlierReplay)
{
    auto& actor = Add("Actor", 20, "EventRelayTraining", 0, 0, false, 1, "Helpful");
    auto& first = Add("First", 5);
    auto& second = Add("Second", 3);
    const auto before = Snapshot();
    const auto initial = transformator_->CurrentState();
    io_.ExpectChoice("First", true);
    io_.ExpectChoice("Second", true);
    rng_.ExpectCall(6, 4);
    const auto first_point = Suspend([&]() { ExecuteJournal(actor, Option(actor, "EventRelay")); });
    const auto second_point = Suspend([&]() { first_point.Resume(); });
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 3);

    TTransformator source_editor(io_);
    const auto sources_before = source_editor.CurrentState();
    auto source_context = source_editor.ExecutionContext();
    source_context.AssignGuarded(actor.GetCreature()->Class(), TClass::Create({}),
                                 TGuarded<TCreature>(*actor.GetCreature()), "class");
    second_point.Resume();
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 9);
    second_point.Revert(*transformator_);
    second_point.Resume();
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 9);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);

    first_point.Revert(*transformator_);
    io_.ExpectChoice("Second", true);
    rng_.ExpectCall(6, 2);
    const auto replayed_second = Suspend([&]() { first_point.Resume(); });
    replayed_second.Resume();
    EXPECT_EQ(second.GetCreature()->Hitpoints()->Current(), 5);
    EXPECT_EQ(first.GetCreature()->Hitpoints()->Current(), 5);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);

    source_editor.Undo(sources_before);
    transformator_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TGeneratedActionsTest, ClassValuedEventContributionsDriveDiceThroughCompiledLoop)
{
    auto& actor = Add("Actor", 20, "DiceMending");
    auto& target = Add("Target", 2);
    io_.ExpectChoice("Target");
    rng_.ExpectCall(8, 3);
    rng_.ExpectCall(4, 1);
    rng_.ExpectCall(4, 4);

    EXPECT_EQ(battle_->Execute(Option(actor, "DiceMend")), EAbilityExecutionResult::Applied);

    EXPECT_EQ(target.GetCreature()->Hitpoints()->Current(), 10);
    EXPECT_EQ(actor.GetCreature()->Actions()->Count(), 2);
}

}

#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_initiative.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/initiative_order.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/transformation/transformator.h>

#include "test_lib/mock_dice_roller.h"
#include "test_lib/mock_interaction_system.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

TStatisticBaseline Fixed(int value)
{
    return TStatisticBaseline::Create({.IsFixed = true, .Value = value});
}

TAbilityScores Scores(int wisdom)
{
    return TAbilityScores::Create({
        .Strength = TAbilityScore::Create({.Value = 10}),
        .Dexterity = TAbilityScore::Create({.Value = 20}),
        .Constitution = TAbilityScore::Create({.Value = 10}),
        .Intelligence = TAbilityScore::Create({.Value = 10}),
        .Wisdom = TAbilityScore::Create({.Value = wisdom}),
        .Charisma = TAbilityScore::Create({.Value = 10}),
    });
}

TCreature::TInitialValues RankedValues()
{
    return {
        .Level = 1, .Characteristic = Scores(12),
        .Proficiency = TProficiency::Create({
            .Perception = TProficiencyValue::Create({.Rank = EProficiencyRank::Expert}),
        }),
    };
}

TCreature::TInitialValues FixedValues(int bonus)
{
    return {
        .PerceptionBaseline = Fixed(bonus),
        .Hitpoints = THitPoints::Create({
            .Maximum = Statistic::Create({.Base = 20}), .Current = 20,
        }),
    };
}

TAstNode Snapshot(const TBattle& battle)
{
    TAstContext context;
    return battle.GetAst(context);
}

class TInitiativeTest : public ::testing::Test {
protected:
    TPlayer& Add(TCreature::TInitialValues values, int roll,
                 const TInitiativeOptions& options = {})
    {
        const auto id = static_cast<int>(players_.size());
        creatures_.push_back(TCreature::Create(std::move(values)));
        players_.emplace_back(&creatures_.back(), TPlayerTeam{id}, TPlayerId{id},
                              "Participant", "");
        rng_.ExpectCall(20, roll);
        order_.AddPlayer(&players_.back(), options);
        return players_.back();
    }

    std::vector<int> TakeRound()
    {
        EXPECT_EQ(order_.CurrentPlayer(), nullptr);
        std::vector<int> ids;
        order_.Next(journal_);
        while (const auto* player = order_.CurrentPlayer()) {
            ids.push_back(player->GetId());
            order_.Next(journal_);
        }
        return ids;
    }

    TAstNode StoredOrder() const
    {
        TAstContext context;
        for (const auto& player : players_) {
            context.RegisterIdentity(&player, "player#" + std::to_string(player.GetId()));
        }
        return order_.GetAst(context);
    }

    TAstNode StateSnapshot() const
    {
        TAstContext context;
        context.RegisterIdentity(&order_, "initiative");
        auto node = TAstNode::MakeObject("initiative_test");
        for (const auto& player : players_) {
            const auto name = "player#" + std::to_string(player.GetId());
            context.RegisterIdentity(&player, name);
            context.RegisterIdentity(player.GetCreature(), name + ".creature");
            AddOwnedObject(node, name, player, context);
        }
        AddOwnedObject(node, "order", order_, context);
        AddOwnedObject(node, "journal", journal_, context);
        return node;
    }

    TMockInteractionSystem io_;
    TMockRng rng_;
    TTransformator journal_{io_};
    TInitiativeOrder order_{&rng_, io_};
    std::deque<TCreature> creatures_;
    std::deque<TPlayer> players_;
};

TEST_F(TInitiativeTest, GeneratedPerceptionQueryUsesRanksTotalsAndTypedModifiers)
{
    auto creature = TCreature::Create(RankedValues());
    const TCombatCalculator calculator;
    EXPECT_EQ(creature.InitiativeBonus(), 6);
    EXPECT_EQ(calculator.InitiativeBonus(creature), 6);
    journal_.ChangeCondition(&creature, EConditionKind::Frightened, 2);
    const auto modifiers = TCheckModifiers::Create({
        .ItemBonus = 2, .StatusPenalty = 1, .CircumstanceBonus = 1,
    });
    const auto before = creature.ToJson();
    EXPECT_EQ(creature.InitiativeBonus(), 4);
    EXPECT_EQ(creature.InitiativeBonusWithModifiers(modifiers), 7);
    EXPECT_EQ(calculator.InitiativeBonus(creature, modifiers), 7);
    EXPECT_EQ(creature.ToJson(), before);

    for (const int total : {0, -3, 7}) {
        auto values = RankedValues();
        values.Level = 10;
        values.PerceptionBaseline = Fixed(total);
        const auto npc = TCreature::Create(std::move(values));
        EXPECT_EQ(npc.InitiativeBonus(), total);
        EXPECT_EQ(calculator.InitiativeBonus(npc), total);
    }
}

TEST_F(TInitiativeTest, DescendingTotalsTreatNaturalOneAndTwentyAsOrdinaryRolls)
{
    Add(FixedValues(0), 20);
    Add(FixedValues(30), 1);
    Add(FixedValues(-50), 20);
    Add(FixedValues(-5), 1);
    EXPECT_EQ(TakeRound(), (std::vector<int>{1, 0, 3, 2}));
    rng_.Verify();
}

TEST_F(TInitiativeTest, EqualTotalsUseExplicitTieOrderThenInsertionIgnoringBonus)
{
    Add(FixedValues(15), 5);
    Add(FixedValues(0), 20);
    Add(FixedValues(10), 10, {.TieOrder = -1});
    Add(FixedValues(5), 15, {.TieOrder = 3});
    Add(FixedValues(21), 1, {.TieOrder = 100});
    EXPECT_EQ(TakeRound(), (std::vector<int>{4, 2, 0, 1, 3}));
    rng_.Verify();
}

TEST_F(TInitiativeTest, SkillSelectionUsesItsOwnTotalAndExplicitModifiers)
{
    auto values = FixedValues(2);
    values.SkillBaselines = std::map<ESkill, TStatisticBaseline>{{ESkill::Stealth, Fixed(20)}};
    const auto modifiers = TCheckModifiers::Create({.CircumstanceBonus = 3});
    Add(values, 10, {.Skill = ESkill::Stealth, .Modifiers = modifiers});
    Add(FixedValues(30), 1);
    Add(values, 10, {.Modifiers = modifiers});
    EXPECT_EQ(TakeRound(), (std::vector<int>{0, 1, 2}));
    rng_.Verify();
}

TEST_F(TInitiativeTest, LaterLevelAttributeAndConditionChangesDoNotRerollStoredOrder)
{
    auto& actor = Add(RankedValues(), 10);
    Add(FixedValues(7), 10);
    auto& creature = *actor.GetCreature();
    const auto start = journal_.CurrentState();
    const auto before = StateSnapshot();
    const auto stored = StoredOrder();
    auto execution = journal_.ExecutionContext();
    execution.AssignGuarded(creature.Level(), 4, TGuarded<TCreature>(creature), "level");
    EXPECT_EQ(creature.InitiativeBonus(), 9);
    execution.AssignGuarded(creature.Characteristic(), Scores(16),
                            TGuarded<TCreature>(creature), "characteristic");
    EXPECT_EQ(creature.InitiativeBonus(), 11);
    journal_.ChangeCondition(&creature, EConditionKind::Frightened, 1);
    EXPECT_EQ(creature.InitiativeBonus(), 10);
    EXPECT_EQ(StoredOrder(), stored);
    EXPECT_EQ(TakeRound(), (std::vector<int>{1, 0}));
    journal_.Undo(start);
    const auto restored = StateSnapshot();
    EXPECT_EQ(restored, before) << restored.DiffWith(before);
    EXPECT_EQ(creature.InitiativeBonus(), 6);
    rng_.Verify();
}

TEST_F(TInitiativeTest, RoundProgressReplaysWithoutRerollAndRestoresExactState)
{
    Add(FixedValues(2), 10);
    Add(FixedValues(7), 10);
    Add(FixedValues(7), 10, {.TieOrder = -1});
    const auto checkpoint = journal_.CurrentState();
    const auto before = StateSnapshot();
    const std::vector<int> expected{2, 1, 0};
    EXPECT_EQ(TakeRound(), expected);
    EXPECT_EQ(order_.CurrentRound(), 1);
    EXPECT_EQ(TakeRound(), expected);
    EXPECT_EQ(order_.CurrentRound(), 2);
    const auto completed = StateSnapshot();
    journal_.Undo(checkpoint);
    EXPECT_EQ(order_.CurrentRound(), 0);
    EXPECT_EQ(order_.CurrentPlayer(), nullptr);
    EXPECT_EQ(StateSnapshot(), before);
    EXPECT_EQ(TakeRound(), expected);
    EXPECT_EQ(TakeRound(), expected);
    EXPECT_EQ(StateSnapshot(), completed);
    rng_.Verify();
}

TEST_F(TInitiativeTest, InvalidOptionsRejectBeforeRollingOrInserting)
{
    Add(FixedValues(0), 10);
    auto creature = TCreature::Create(FixedValues(1));
    TPlayer candidate(&creature, TPlayerTeam{1}, TPlayerId{1}, "Invalid", "");
    const auto before = StoredOrder();
    rng_.ExpectCall(20, 5);
    EXPECT_THROW(order_.AddPlayer(nullptr), std::invalid_argument);
    EXPECT_THROW(order_.AddPlayer(&candidate, {
        .Modifiers = TCheckModifiers::Create({.StatusPenalty = -1}),
    }), std::runtime_error);
    EXPECT_THROW(order_.AddPlayer(&candidate, {.Skill = static_cast<ESkill>(1000)}),
                 std::runtime_error);
    EXPECT_EQ(rng_.RemainingCalls(), 1);
    EXPECT_EQ(StoredOrder(), before);
    EXPECT_NO_THROW(order_.AddPlayer(&candidate));
    rng_.Verify();
}

TEST_F(TInitiativeTest, ActedAndUnactedParticipantsMoveWithoutChangingCurrentTurnAndReplayExactly)
{
    auto& first = Add(FixedValues(2), 20);
    Add(FixedValues(3), 15);
    auto& current = Add(FixedValues(4), 10, {.TieOrder = 7});
    Add(FixedValues(1), 5);
    auto& last = Add(FixedValues(-2), 1);
    order_.Next(journal_);
    order_.Next(journal_);
    order_.Next(journal_);
    ASSERT_EQ(order_.CurrentPlayer(), &current);
    const auto checkpoint = journal_.CurrentState();
    const auto before = StateSnapshot();
    const auto run = [&] {
        order_.MoveBefore(&first, &current, journal_);
        order_.MoveBefore(&last, &current, journal_);
        EXPECT_EQ(order_.CurrentPlayer(), &current);
        EXPECT_EQ(order_.GetCurrentPosition(), 3);
        EXPECT_EQ(order_.CurrentRound(), 1);
        order_.Next(journal_);
        ASSERT_NE(order_.CurrentPlayer(), nullptr);
        EXPECT_EQ(order_.CurrentPlayer()->GetId(), 3);
        order_.Next(journal_);
        EXPECT_EQ(TakeRound(), (std::vector<int>{1, 0, 4, 2, 3}));
    };
    run();
    const auto completed = StateSnapshot();
    journal_.Undo(checkpoint);
    EXPECT_EQ(StateSnapshot(), before);
    EXPECT_EQ(order_.CurrentPlayer(), &current);
    run();
    EXPECT_EQ(StateSnapshot(), completed);
    journal_.Undo(checkpoint);
    EXPECT_EQ(StateSnapshot(), before);
    rng_.Verify();
}

TEST_F(TInitiativeTest, MovingTheCurrentEntryPreservesItsIteratorAndRestoresPreviousSuccessor)
{
    auto& first = Add(FixedValues(0), 20);
    auto& current = Add(FixedValues(0), 15);
    auto& last = Add(FixedValues(0), 10);
    order_.Next(journal_);
    order_.Next(journal_);
    const auto checkpoint = journal_.CurrentState();
    const auto before = StateSnapshot();
    order_.MoveBefore(&current, &first, journal_);
    EXPECT_EQ(order_.CurrentPlayer(), &current);
    EXPECT_EQ(order_.GetCurrentPosition(), 0);
    order_.MoveBefore(&last, &current, journal_);
    EXPECT_EQ(order_.CurrentPlayer(), &current);
    EXPECT_EQ(order_.GetCurrentPosition(), 1);
    order_.Next(journal_);
    EXPECT_EQ(order_.CurrentPlayer(), &first);
    order_.Next(journal_);
    EXPECT_EQ(TakeRound(), (std::vector<int>{2, 1, 0}));
    journal_.Undo(checkpoint);
    EXPECT_EQ(StateSnapshot(), before);
    rng_.Verify();
}

TEST_F(TInitiativeTest, SelfAndAdjacentRelocationArePureAndInvalidMembersCannotChangeTheJournal)
{
    auto& first = Add(FixedValues(0), 20);
    auto& second = Add(FixedValues(0), 15);
    order_.Next(journal_);
    order_.Next(journal_);
    auto foreign_creature = TCreature::Create(FixedValues(0));
    TPlayer foreign(&foreign_creature, TPlayerTeam{0}, TPlayerId{0}, "Foreign", "");
    const auto before = StateSnapshot();
    order_.MoveBefore(&second, &second, journal_);
    order_.MoveBefore(&first, &second, journal_);
    EXPECT_EQ(StateSnapshot(), before);
    EXPECT_THROW(order_.MoveBefore(&foreign, &second, journal_), std::invalid_argument);
    EXPECT_THROW(order_.MoveBefore(&first, &foreign, journal_), std::invalid_argument);
    EXPECT_THROW(order_.MoveBefore(nullptr, &second, journal_), std::invalid_argument);
    EXPECT_THROW(journal_.MoveInitiativeBefore(nullptr, &first, &second), std::invalid_argument);
    EXPECT_THROW(order_.SetCurrentPosition(3), std::out_of_range);
    EXPECT_EQ(StateSnapshot(), before);
    rng_.Verify();
}

TEST_F(TInitiativeTest, EnrollmentRejectsDuplicateIdentityAndLateArrivalBeforeRolling)
{
    auto& first = Add(FixedValues(0), 10);
    auto creature = TCreature::Create(FixedValues(1));
    TPlayer candidate(&creature, TPlayerTeam{1}, TPlayerId{1}, "Candidate", "");
    TPlayer duplicate_id(&creature, TPlayerTeam{1}, TPlayerId{0}, "Duplicate", "");
    TPlayer duplicate_creature(first.GetCreature(), TPlayerTeam{1}, TPlayerId{1}, "Duplicate", "");
    const auto checkpoint = journal_.CurrentState();
    const auto before = StateSnapshot();
    EXPECT_THROW(order_.AddPlayer(&first), std::invalid_argument);
    EXPECT_THROW(order_.AddPlayer(&duplicate_id), std::invalid_argument);
    EXPECT_THROW(order_.AddPlayer(&duplicate_creature), std::invalid_argument);
    EXPECT_EQ(StateSnapshot(), before);
    order_.Next(journal_);
    const auto active = StateSnapshot();
    EXPECT_THROW(order_.AddPlayer(&candidate), std::invalid_argument);
    EXPECT_EQ(StateSnapshot(), active);
    order_.Next(journal_);
    EXPECT_EQ(order_.CurrentPlayer(), nullptr);
    const auto round_end = StateSnapshot();
    EXPECT_THROW(order_.AddPlayer(&candidate), std::invalid_argument);
    EXPECT_EQ(StateSnapshot(), round_end);
    journal_.Undo(checkpoint);
    EXPECT_EQ(StateSnapshot(), before);
    rng_.ExpectCall(20, 15);
    EXPECT_NO_THROW(order_.AddPlayer(&candidate));
    rng_.Verify();
}

TEST(InitiativeBattleTest, AddPlayerForwardsSkillModifiersAndExplicitTieOrder)
{
    TMockInteractionSystem io;
    TMockRng rng;
    auto rules = std::make_shared<TAbilityRules>();
    rules->Catalog.Register({"initiative.Probe", EAbilityKind::Action, {}});
    rules->Catalog.Register({"initiative.Training", EAbilityKind::Feature, {"initiative.Probe"}});
    rules->Executions.Register("initiative.Probe", std::make_shared<TAction>(
        [](std::shared_ptr<TActionContext>) {}, TAction::TBoundResources{}, "Probe"));
    nlohmann::json map = {{"x_size", 3}, {"y_size", 1}};
    TBattle battle(TBattleMap(map), &rng, io, std::move(rules));
    std::deque<TCreature> creatures;
    for (int id = 0; id < 3; ++id) {
        auto values = FixedValues(id == 0 ? 0 : 23 - id);
        values.SkillBaselines = std::map<ESkill, TStatisticBaseline>{{ESkill::Stealth, Fixed(20)}};
        TIdCollection<TFeatureSelection> features;
        features.Add(TFeatureSelection("initiative.Training"));
        values.Features = std::move(features);
        creatures.push_back(TCreature::Create(std::move(values)));
        TInitiativeOptions options;
        if (id == 0) {
            options.Skill = ESkill::Stealth;
            options.Modifiers = TCheckModifiers::Create({.CircumstanceBonus = 2});
            options.TieOrder = 1;
        }
        rng.ExpectCall(20, 5);
        battle.AddPlayer(TPlayer(&creatures.back(), TPlayerTeam{id == 0 ? 0 : 1},
                                TPlayerId{id}, "Participant", ""), {id, 0}, options);
    }
    io.ExpectChoice(1, "next action", "End of turn");
    io.ExpectChoice(0, "next action", "End of turn");
    io.ExpectChoice(2, "next action", "End of turn");
    EXPECT_THROW(battle.StartBattle(), TTooManyCallsError);
    io.Verify();
    rng.Verify();
}

TEST(InitiativeBattleTest, RollOverflowAndInvalidDiceLeaveMapPlayersAndOrderUnchanged)
{
    TMockInteractionSystem io;
    TMockRng rng;
    nlohmann::json map = {{"x_size", 2}, {"y_size", 1}};
    TBattle battle(TBattleMap(map), &rng, io);
    auto first = TCreature::Create(FixedValues(0));
    rng.ExpectCall(20, 10);
    battle.AddPlayer(TPlayer(&first, TPlayerTeam{0}, TPlayerId{0}, "First", ""), {0, 0});
    const auto before = Snapshot(battle);
    auto overflow = TCreature::Create(FixedValues(std::numeric_limits<int>::max()));
    rng.ExpectCall(20, 1);
    EXPECT_THROW(battle.AddPlayer(TPlayer(&overflow, TPlayerTeam{1}, TPlayerId{1}, "Overflow", ""),
                                 {1, 0}), std::overflow_error);
    EXPECT_EQ(Snapshot(battle), before);
    auto candidate = TCreature::Create(FixedValues(0));
    for (const int roll : {0, 21}) {
        rng.ExpectCall(20, roll);
        EXPECT_THROW(battle.AddPlayer(TPlayer(&candidate, TPlayerTeam{1}, TPlayerId{1}, "Invalid", ""),
                                     {1, 0}), std::out_of_range);
        EXPECT_EQ(Snapshot(battle), before);
        EXPECT_EQ(battle.BattleMap()->GetCell(1, 0).player, nullptr);
    }
    rng.ExpectCall(20, 20);
    EXPECT_NO_THROW(battle.AddPlayer(TPlayer(&candidate, TPlayerTeam{1}, TPlayerId{1}, "Valid", ""),
                                    {1, 0}));
    EXPECT_EQ(battle.GetIfPlayers([](const TPlayer*) { return true; }).size(), 2);
    rng.Verify();
}

class TInitiativeRuntimeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto rules = std::make_shared<TAbilityRules>();
        rules->Catalog.Register({"initiative.Probe", EAbilityKind::Action, {}});
        rules->Catalog.Register({"initiative.Training", EAbilityKind::Feature, {"initiative.Probe"}});
        rules->Executions.Register("initiative.Probe", std::make_shared<TAction>(
            [](std::shared_ptr<TActionContext>) {}, TAction::TBoundResources{}, "Probe"));
        nlohmann::json map = {{"x_size", 5}, {"y_size", 1}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, std::move(rules));
        for (int id = 0; id < 4; ++id) {
            auto values = FixedValues(0);
            TIdCollection<TFeatureSelection> features;
            features.Add(TFeatureSelection("initiative.Training"));
            values.Features = std::move(features);
            creatures_.push_back(TCreature::Create(std::move(values)));
            rng_.ExpectCall(20, 20 - id * 5);
            battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id % 2}, TPlayerId{id},
                                        "Participant", ""), {id, 0});
        }
        players_ = battle_->GetIfPlayers([](const TPlayer*) { return true; });
    }

    std::shared_ptr<TActionContext> Context(TPlayer& actor)
    {
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = &journal_,
            .io_system = &io_, .scheduler = nullptr, .effect_manager = nullptr,
            .game_object_registry = std::make_shared<TGameObjectRegistry>(),
        });
        context->game_object_registry->Add(TGameObjectIdManager::Instance().Register("self"), &actor);
        return context;
    }

    TAstNode StateSnapshot() const
    {
        TAstContext context;
        auto node = TAstNode::MakeObject("initiative_runtime");
        AddOwnedObject(node, "battle", *battle_, context);
        AddOwnedObject(node, "journal", journal_, context);
        return node;
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
    }

    TMockInteractionSystem io_;
    TMockRng rng_;
    TTransformator journal_{io_};
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    std::vector<TPlayer*> players_;
};

TEST_F(TInitiativeRuntimeTest, SetupIsNoopOnlyAfterActorSourceMembershipAndWriteValidation)
{
    const auto context = Context(*players_[0]);
    const auto target = BindGeneratedActionEntity(*players_[2], battle_.get());
    auto foreign_creature = TCreature::Create(FixedValues(0));
    TPlayer foreign(&foreign_creature, TPlayerTeam{0}, TPlayerId{2}, "Foreign", "");
    const auto before = StateSnapshot();
    ASSERT_EQ(battle_->CurrentTurnPlayer(), nullptr);
    EXPECT_NO_THROW(MoveActionEntityBeforeCurrentTurn(context, target));
    EXPECT_THROW(MoveActionEntityBeforeCurrentTurn(context, target.ReadOnly()), std::logic_error);
    EXPECT_THROW(MoveActionEntityBeforeCurrentTurn(context, BindGeneratedActionEntity(foreign)),
                 TAbilityUnavailable);
    EXPECT_THROW(battle_->MoveBeforeCurrentTurn(foreign, journal_), std::invalid_argument);
    EXPECT_THROW(MoveActionEntityBeforeCurrentTurn(Context(foreign), target), TAbilityUnavailable);
    context->validate_ability = [] { return false; };
    EXPECT_THROW(MoveActionEntityBeforeCurrentTurn(context, target), TAbilityUnavailable);
    context->validate_ability = {};
    context->battle = nullptr;
    EXPECT_THROW(MoveActionEntityBeforeCurrentTurn(context, target), TAbilityUnavailable);
    EXPECT_THROW(MoveActionEntityBeforeCurrentTurn(nullptr, target), std::invalid_argument);
    EXPECT_EQ(StateSnapshot(), before);
}

TEST_F(TInitiativeRuntimeTest, ReactionActorRelocatesBeforeActualCurrentTurnWithExactReplay)
{
    io_.ExpectChoice(0, "next action", "End of turn");
    io_.AddCheckCallback([&] {
        ASSERT_EQ(battle_->CurrentTurnPlayer(), players_[1]);
        const auto checkpoint = journal_.CurrentState();
        const auto before = StateSnapshot();
        const auto context = Context(*players_[3]);
        const auto target = BindGeneratedActionEntity(*players_[2], battle_.get());
        MoveActionEntityBeforeCurrentTurn(context, target);
        EXPECT_EQ(battle_->CurrentTurnPlayer(), players_[1]);
        const auto relocated = StateSnapshot();
        EXPECT_NE(relocated, before);
        MoveActionEntityBeforeCurrentTurn(context, target);
        MoveActionEntityBeforeCurrentTurn(context, BindGeneratedActionEntity(*players_[1], battle_.get()));
        EXPECT_EQ(StateSnapshot(), relocated);
        journal_.Undo(checkpoint);
        EXPECT_EQ(StateSnapshot(), before);
        MoveActionEntityBeforeCurrentTurn(context, target);
        EXPECT_EQ(StateSnapshot(), relocated);
    });
    for (const int id : {1, 3, 0, 2, 1, 3}) {
        io_.ExpectChoice(id, "next action", "End of turn");
    }
    EXPECT_THROW(battle_->StartBattle(), TTooManyCallsError);
}

TEST_F(TInitiativeRuntimeTest, BattleEnrollmentFailsBeforeRollingOrMovingAfterTurnStart)
{
    auto candidate = TCreature::Create(FixedValues(0));
    io_.AddCheckCallback([&] {
        ASSERT_EQ(battle_->CurrentTurnPlayer(), players_[0]);
        const auto before = StateSnapshot();
        EXPECT_THROW(battle_->AddPlayer(TPlayer(&candidate, TPlayerTeam{1}, TPlayerId{4},
                                                "Late", ""), {4, 0}), std::invalid_argument);
        EXPECT_EQ(StateSnapshot(), before);
        EXPECT_EQ(battle_->BattleMap()->GetCell(4, 0).player, nullptr);
    });
    io_.ExpectChoice(0, "next action", "End of turn");
    EXPECT_THROW(battle_->StartBattle(), TTooManyCallsError);
}

}

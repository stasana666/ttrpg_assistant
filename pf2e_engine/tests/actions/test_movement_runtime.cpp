#include <pf2e_engine/actions/generated_movement.h>
#include <pf2e_engine/actions/generated_world.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

#include "../test_lib/mock_dice_roller.h"
#include "../test_lib/mock_interaction_system.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

TCreature MovementCreature()
{
    return TCreature::Create({
        .Hitpoints = THitPoints::Create({
            .Maximum = Statistic::Create({.Base = 10}), .Current = 10,
        }),
        .Actions = TResource(5),
    });
}

class TMovementRuntimeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        nlohmann::json map = {{"x_size", 5}, {"y_size", 5}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_);
        journal_ = std::make_unique<TTransformator>(io_);
        actor_ = &Add({2, 2});
        other_ = &Add({4, 4});
        context_ = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = journal_.get(),
            .io_system = &io_, .scheduler = nullptr, .effect_manager = nullptr,
        });
        context_->game_object_registry = std::make_shared<TGameObjectRegistry>();
        context_->game_object_registry->Add(
            TGameObjectIdManager::Instance().Register("self"), actor_);
        io_.SetReactionSuspends(true);
    }

    TPlayer& Add(TPosition position)
    {
        const int id = static_cast<int>(creatures_.size());
        creatures_.push_back(MovementCreature());
        rng_.ExpectCall(20, 10);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id}, TPlayerId{id},
                                   "Actor " + std::to_string(id), ""), position);
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    TGeneratedEntityView ActorView() const
    {
        return BindGeneratedActionEntity(*actor_, battle_.get());
    }

    TAstNode Snapshot() const
    {
        TAstContext context;
        auto node = TAstNode::MakeObject("movement_execution");
        AddOwnedObject(node, "battle", *battle_, context);
        AddOwnedObject(node, "journal", *journal_, context);
        return node;
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
    }

    TMockRng rng_;
    TMockInteractionSystem io_;
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> journal_;
    std::shared_ptr<TActionContext> context_;
    TPlayer* actor_ = nullptr;
    TPlayer* other_ = nullptr;
};

TEST_F(TMovementRuntimeTest, AdjacentPositionsAreBoundedDeterministicValuesAndQueriesArePure)
{
    const TGeneratedWorldView world(battle_.get());
    const auto before = Snapshot();
    EXPECT_EQ(world.AdjacentPositions({2, 2}), (std::vector<TPosition>{
        {1, 1}, {2, 1}, {3, 1}, {1, 2}, {3, 2}, {1, 3}, {2, 3}, {3, 3},
    }));
    EXPECT_EQ(world.AdjacentPositions({0, 0}), (std::vector<TPosition>{
        {1, 0}, {0, 1}, {1, 1},
    }));
    const auto readonly = world.Bind<TCreature>(*actor_);
    EXPECT_TRUE(world.Contains({0, 0}));
    EXPECT_TRUE(world.Contains({4, 4}));
    EXPECT_FALSE(world.Contains({-1, 0}));
    EXPECT_FALSE(world.Contains({5, 4}));
    EXPECT_TRUE(world.CanOccupy(readonly.View(), {2, 2}));
    EXPECT_TRUE(world.CanOccupy(readonly.View(), {3, 2}));
    EXPECT_FALSE(world.CanOccupy(readonly.View(), {4, 4}));
    EXPECT_FALSE(world.CanOccupy(readonly.View(), {-1, 2}));
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TMovementRuntimeTest, DiagonalAndOrthogonalMovesUpdateMapAndUndoWithoutCostsOrReactions)
{
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    const auto old_map = battle_->BattleMap();
    MoveActionEntity(context_, ActorView(), {2, 2}, {3, 3});
    EXPECT_EQ(actor_->GetPosition(), (TPosition{3, 3}));
    EXPECT_EQ(battle_->BattleMap()->GetCell(2, 2).player, nullptr);
    EXPECT_EQ(battle_->BattleMap()->GetCell(3, 3).player, actor_);
    EXPECT_EQ(old_map->GetCell(2, 2).player, actor_);
    EXPECT_EQ(old_map->GetCell(3, 3).player, nullptr);
    MoveActionEntity(context_, ActorView(), {3, 3}, {4, 3});
    EXPECT_EQ(actor_->GetPosition(), (TPosition{4, 3}));
    EXPECT_EQ(actor_->GetCreature()->Actions()->Count(), 5);
    journal_->Undo(initial);
    EXPECT_EQ(actor_->GetPosition(), (TPosition{2, 2}));
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TMovementRuntimeTest, RejectsStaleOriginsNonAdjacentAndOccupiedDestinationsBeforeMutation)
{
    const auto before = Snapshot();
    EXPECT_THROW(MoveActionEntity(context_, ActorView(), {1, 2}, {2, 3}), TAbilityUnavailable);
    EXPECT_THROW(MoveActionEntity(context_, ActorView(), {2, 2}, {2, 2}), TAbilityUnavailable);
    EXPECT_THROW(MoveActionEntity(context_, ActorView(), {2, 2}, {0, 0}), TAbilityUnavailable);
    EXPECT_THROW(MoveActionEntity(context_, ActorView(), {2, 2}, {-1, 2}), TAbilityUnavailable);
    EXPECT_EQ(Snapshot(), before);
    journal_->MovePlayer(other_, {3, 2});
    const auto occupied = Snapshot();
    EXPECT_THROW(MoveActionEntity(context_, ActorView(), {2, 2}, {3, 2}), TAbilityUnavailable);
    EXPECT_EQ(Snapshot(), occupied);
}

TEST_F(TMovementRuntimeTest, RejectsReadonlyForeignAndUnboundCapabilities)
{
    const auto before = Snapshot();
    const TGeneratedWorldView world(battle_.get());
    const auto readonly = world.Bind<TCreature>(*actor_);
    EXPECT_THROW(MoveActionEntity(context_, readonly.View(), {2, 2}, {3, 2}), std::logic_error);
    nlohmann::json map = {{"x_size", 5}, {"y_size", 5}};
    TBattle foreign(TBattleMap(map), &rng_, io_);
    auto foreign_creature = MovementCreature();
    rng_.ExpectCall(20, 10);
    foreign.AddPlayer(TPlayer(&foreign_creature, TPlayerTeam{0}, TPlayerId{0}, "Foreign", ""), {2, 2});
    auto* foreign_player = foreign.GetIfPlayers([](const TPlayer*) { return true; }).at(0);
    const auto foreign_view = BindGeneratedActionEntity(*foreign_player, &foreign);
    EXPECT_THROW(MoveActionEntity(context_, foreign_view, {2, 2}, {3, 2}), TAbilityUnavailable);
    EXPECT_THROW(world.CanOccupy(foreign_view, {3, 2}), TAbilityUnavailable);
    EXPECT_EQ(foreign_player->GetPosition(), (TPosition{2, 2}));
    TPlayer detached(actor_->GetCreature(), TPlayerTeam{0}, TPlayerId{0}, "Detached", "");
    const auto detached_view = BindGeneratedActionEntity(detached);
    EXPECT_THROW(MoveActionEntity(context_, detached_view, {0, 0}, {1, 0}), TAbilityUnavailable);
    EXPECT_THROW(MoveActionEntity(nullptr, ActorView(), {2, 2}, {3, 2}), std::invalid_argument);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TMovementRuntimeTest, RequiresLiveActorAndGrantButDoesNotFilterOtherMovableEntities)
{
    const auto before = Snapshot();
    context_->validate_ability = [] { return false; };
    EXPECT_THROW(MoveActionEntity(context_, ActorView(), {2, 2}, {3, 2}), TAbilityUnavailable);
    EXPECT_EQ(Snapshot(), before);
    context_->validate_ability = {};
    const auto initial = journal_->CurrentState();
    journal_->DealDamage(actor_, 10);
    const auto dead_actor = Snapshot();
    EXPECT_THROW(MoveActionEntity(context_, ActorView(), {2, 2}, {3, 2}), TAbilityUnavailable);
    EXPECT_EQ(Snapshot(), dead_actor);
    journal_->Undo(initial);
    journal_->DealDamage(other_, 10);
    MoveActionEntity(context_, BindGeneratedActionEntity(*other_, battle_.get()), {4, 4}, {3, 4});
    EXPECT_EQ(other_->GetPosition(), (TPosition{3, 4}));
    EXPECT_FALSE(other_->GetCreature()->IsAlive());
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TMovementRuntimeTest, AReplayedMoveRevalidatesOriginAndRestoresTheSameMap)
{
    const auto before = Snapshot();
    const auto initial = journal_->CurrentState();
    const TSavepointStackUnwind point(initial, [this, entity = ActorView()] {
        MoveActionEntity(context_, entity, {2, 2}, {3, 3});
    });
    journal_->MovePlayer(actor_, {1, 2});
    const auto changed_origin = Snapshot();
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Snapshot(), changed_origin);
    point.Revert(*journal_);
    point.Resume();
    const auto completed = Snapshot();
    point.Revert(*journal_);
    EXPECT_EQ(Snapshot(), before);
    point.Resume();
    EXPECT_EQ(Snapshot(), completed);
    point.Revert(*journal_);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TMovementRuntimeTest, FailedLegacyMoveConstructorsLeaveHistoryAndMapUntouched)
{
    const auto before = Snapshot();
    for (const TPosition destination : std::vector<TPosition>{
            {-1, 0}, {5, 0}, {0, 5},
            {std::numeric_limits<int>::max(), std::numeric_limits<int>::min()}}) {
        EXPECT_THROW(journal_->MovePlayer(actor_, destination), std::out_of_range);
        EXPECT_EQ(Snapshot(), before);
    }
    EXPECT_THROW(journal_->MovePlayer(actor_, {4, 4}), std::runtime_error);
    EXPECT_THROW(TMovePlayer(actor_, {4, 4}), std::runtime_error);
    EXPECT_THROW(journal_->MovePlayer(nullptr, {0, 0}), std::invalid_argument);
    TPlayer detached(actor_->GetCreature(), TPlayerTeam{0}, TPlayerId{2}, "Detached", "");
    EXPECT_THROW(journal_->MovePlayer(&detached, {0, 0}), std::logic_error);
    EXPECT_EQ(Snapshot(), before);
    const auto initial = journal_->CurrentState();
    journal_->MovePlayer(actor_, {0, 0});
    EXPECT_EQ(actor_->GetPosition(), (TPosition{0, 0}));
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TMovementRuntimeTest, MultiplePlayersCanReuseVacatedCellsAndUndoInReverseOrder)
{
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    MoveActionEntity(context_, ActorView(), {2, 2}, {3, 2});
    journal_->MovePlayer(other_, {2, 2});
    EXPECT_EQ(battle_->BattleMap()->GetCell(2, 2).player, other_);
    EXPECT_EQ(battle_->BattleMap()->GetCell(3, 2).player, actor_);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TMovementRuntimeTest, InvalidAndOccupiedBattleInsertionDoesNotChangeExistingParticipants)
{
    const auto before = Snapshot();
    auto creature = MovementCreature();
    EXPECT_THROW(battle_->AddPlayer(TPlayer(&creature, TPlayerTeam{0}, TPlayerId{2}, "Outside", ""),
                                    {-1, 0}), std::out_of_range);
    EXPECT_THROW(battle_->AddPlayer(TPlayer(&creature, TPlayerTeam{0}, TPlayerId{2}, "Occupied", ""),
                                    {2, 2}), std::runtime_error);
    EXPECT_THROW(actor_->BindWith(battle_->BattleMapMutable(), {0, 0}), std::logic_error);
    EXPECT_EQ(battle_->GetIfPlayers([](const TPlayer*) { return true; }).size(), 2u);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TMovementRuntimeTest, UnboundWorldAndInvalidOriginsRejectWithoutOverflow)
{
    const TGeneratedWorldView unbound;
    EXPECT_THROW(unbound.Contains({0, 0}), std::invalid_argument);
    EXPECT_THROW(unbound.AdjacentPositions({0, 0}), std::invalid_argument);
    EXPECT_THROW(unbound.CanOccupy(ActorView(), {0, 0}), std::invalid_argument);
    const TGeneratedWorldView world(battle_.get());
    EXPECT_FALSE(world.Contains({std::numeric_limits<int>::max(), 0}));
    EXPECT_FALSE(world.Contains({0, std::numeric_limits<int>::min()}));
    EXPECT_THROW(world.AdjacentPositions({std::numeric_limits<int>::max(), 0}), std::out_of_range);
    EXPECT_THROW(world.AdjacentPositions({0, std::numeric_limits<int>::min()}), std::out_of_range);
}

TEST(MovementMapTest, MapAccessChecksBothMutableAndConstCoordinates)
{
    nlohmann::json values = {{"x_size", 2}, {"y_size", 1}};
    TBattleMap map(values);
    EXPECT_TRUE(map.Contains({1, 0}));
    EXPECT_FALSE(map.Contains({2, 0}));
    EXPECT_FALSE(map.Contains({0, 1}));
    EXPECT_EQ(map.GetCell(1, 0).player, nullptr);
    EXPECT_THROW(map.GetCell(-1, 0), std::out_of_range);
    EXPECT_THROW(map.GetCell(2, 0), std::out_of_range);
    EXPECT_THROW(std::as_const(map).GetCell(0, -1), std::out_of_range);
    EXPECT_THROW(std::as_const(map).GetCell(0, 1), std::out_of_range);
}

TEST(MovementMapTest, InvalidDimensionsAreRejectedBeforeAllocation)
{
    for (const nlohmann::json& invalid : std::vector<nlohmann::json>{
            0, -1, 1.5, "2", std::numeric_limits<std::uint64_t>::max()}) {
        nlohmann::json values = {{"x_size", invalid}, {"y_size", 1}};
        EXPECT_THROW(TBattleMap map(values), std::invalid_argument);
        values = {{"x_size", 1}, {"y_size", invalid}};
        EXPECT_THROW(TBattleMap map(values), std::invalid_argument);
    }
}

}

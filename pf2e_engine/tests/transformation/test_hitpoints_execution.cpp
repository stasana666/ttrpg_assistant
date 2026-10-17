#include <generated_hitpoints_fixture.h>

#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/generated_execution.h>
#include <pf2e_engine/inventory/hitpoints.h>

#include "../ast/ast_test_fixture.h"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

class HitPointExecutionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fixture_ = MakeTwoWarriorBattle();
        transformator_ = std::make_unique<TTransformator>(fixture_->io);
        auto players = fixture_->battle->GetIfPlayers(
            [](const TPlayer* player) { return player->GetId() == 0; });
        player_ = players.front();
    }

    const THitPoints& Hitpoints() const
    {
        return player_->GetCreature()->Hitpoints().Get();
    }

    TAstNode Snapshot() const
    {
        TAstContext ctx;
        TAstNode node = TAstNode::MakeObject("execution");
        AddOwnedObject(node, "journal", *transformator_, ctx);
        AddOwnedObject(node, "battle", *fixture_->battle, ctx);
        return node;
    }

    std::unique_ptr<TTestBattle> fixture_;
    std::unique_ptr<TTransformator> transformator_;
    TPlayer* player_ = nullptr;
};

TEST_F(HitPointExecutionTest, TemporaryHitPointsAbsorbDamageBeforeCurrent)
{
    auto ctx = transformator_->ExecutionContext();
    auto hp = player_->GetCreature()->Hitpoints();
    ctx.Invoke(hp, &THitPoints::ReplaceTemporary, 5);
    const auto before = Snapshot();
    const auto checkpoint = ctx.GetState();

    const TDamageResult result = ctx.Invoke(hp, &THitPoints::DealDamage, 8);

    EXPECT_EQ(result.Incoming(), 8);
    EXPECT_EQ(result.AbsorbedTemporary(), 5);
    EXPECT_EQ(result.Before(), 21);
    EXPECT_EQ(result.After(), 18);
    EXPECT_EQ(result.LostCurrent(), 3);
    EXPECT_EQ(result.Remaining(), 3);
    EXPECT_EQ(Hitpoints().Current(), 18);
    EXPECT_EQ(Hitpoints().Temporary(), 0);
    EXPECT_NE(Snapshot().PrettyPrint().find("ref:player#0.creature.hitpoints"), std::string::npos);

    ctx.Undo(checkpoint);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(HitPointExecutionTest, FullyAbsorbedDamagePreservesCurrent)
{
    auto ctx = transformator_->ExecutionContext();
    auto hp = player_->GetCreature()->Hitpoints();
    ctx.Invoke(hp, &THitPoints::ReplaceTemporary, 5);

    const TDamageResult result = ctx.Invoke(hp, &THitPoints::DealDamage, 3);

    EXPECT_EQ(result.AbsorbedTemporary(), 3);
    EXPECT_EQ(result.LostCurrent(), 0);
    EXPECT_EQ(result.Remaining(), 0);
    EXPECT_EQ(Hitpoints().Current(), 21);
    EXPECT_EQ(Hitpoints().Temporary(), 2);
}

TEST_F(HitPointExecutionTest, EngineAdaptersUseGeneratedDamageAndHealing)
{
    auto ctx = transformator_->ExecutionContext();
    ctx.Invoke(player_->GetCreature()->Hitpoints(), &THitPoints::ReplaceTemporary, 5);
    const auto checkpoint = ctx.GetState();
    const auto before = Snapshot();

    transformator_->DealDamage(player_, 100);
    EXPECT_EQ(Hitpoints().Current(), 0);
    EXPECT_EQ(Hitpoints().Temporary(), 0);
    EXPECT_FALSE(player_->GetCreature()->IsAlive());

    transformator_->Heal(player_, 100);
    EXPECT_EQ(Hitpoints().Current(), 21);
    EXPECT_TRUE(player_->GetCreature()->IsAlive());

    ctx.Undo(checkpoint);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(HitPointExecutionTest, HealReturnsActualRestorationAndPreservesTemporary)
{
    auto ctx = transformator_->ExecutionContext();
    auto hp = player_->GetCreature()->Hitpoints();
    ctx.Invoke(hp, &THitPoints::DealDamage, 8);
    ctx.Invoke(hp, &THitPoints::ReplaceTemporary, 5);

    EXPECT_EQ(ctx.Invoke(hp, &THitPoints::Heal, 100), 8);
    EXPECT_EQ(Hitpoints().Current(), 21);
    EXPECT_EQ(Hitpoints().Temporary(), 5);
    EXPECT_EQ(ctx.Invoke(hp, &THitPoints::Heal, 1), 0);
}

TEST_F(HitPointExecutionTest, ReplacingTemporaryDoesNotAddOrKeepLargerValue)
{
    auto ctx = transformator_->ExecutionContext();
    auto hp = player_->GetCreature()->Hitpoints();
    ctx.Invoke(hp, &THitPoints::ReplaceTemporary, 5);

    ctx.Invoke(hp, &THitPoints::ReplaceTemporary, 2);
    EXPECT_EQ(Hitpoints().Temporary(), 2);
    ctx.Invoke(hp, &THitPoints::ReplaceTemporary, 0);
    EXPECT_EQ(Hitpoints().Temporary(), 0);
    EXPECT_EQ(Hitpoints().Current(), 21);
}

TEST_F(HitPointExecutionTest, InvalidMethodArgumentsLeaveStateAndJournalUnchanged)
{
    auto ctx = transformator_->ExecutionContext();
    auto hp = player_->GetCreature()->Hitpoints();
    ctx.Invoke(hp, &THitPoints::DealDamage, 4);
    ctx.Invoke(hp, &THitPoints::ReplaceTemporary, 3);
    const auto before = Snapshot();

    EXPECT_THROW(transformator_->DealDamage(player_, -1), std::runtime_error);
    EXPECT_THROW(transformator_->Heal(player_, -1), std::runtime_error);
    EXPECT_THROW(ctx.Invoke(hp, &THitPoints::ReplaceTemporary, -1), std::runtime_error);

    EXPECT_EQ(Snapshot(), before);
}

TEST_F(HitPointExecutionTest, GeneratedNestedMethodFailureRollsBackInnerCommit)
{
    TGeneratedHitpointOwner owner = TGeneratedHitpointOwner::Create({.Hitpoints = Hitpoints()});
    auto ctx = transformator_->ExecutionContext();
    ctx.Invoke(owner.Hitpoints(), &THitPoints::ReplaceTemporary, 5);
    auto snapshot = [&]() {
        TAstContext ast_ctx;
        ast_ctx.RegisterIdentity(&owner, "owner");
        TAstNode node = TAstNode::MakeObject("execution");
        AddOwnedObject(node, "journal", *transformator_, ast_ctx);
        AddOwnedObject(node, "owner", owner, ast_ctx);
        return node;
    };
    const auto before = snapshot();

    EXPECT_THROW(ctx.Invoke(TGuarded<TGeneratedHitpointOwner>(owner),
                            &TGeneratedHitpointOwner::DealDamageAndFail, 9),
                 std::runtime_error);

    EXPECT_EQ(snapshot(), before);
    EXPECT_EQ(ctx.Invoke(TGuarded<TGeneratedHitpointOwner>(owner),
                         &TGeneratedHitpointOwner::DealDamageAndReport, 9), 4);
    const std::string ast = snapshot().PrettyPrint();
    EXPECT_NE(ast.find("ref:owner"), std::string::npos);
    EXPECT_NE(ast.find("string:current"), std::string::npos);
}

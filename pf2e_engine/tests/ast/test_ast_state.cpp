#include "ast_test_fixture.h"

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_node.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace {
TAstNode Snapshot(const TBattle& battle)
{
    TAstContext ctx;
    return battle.GetAst(ctx);
}
}

TEST(AstState, EqualityBetweenIdenticalBattles)
{
    auto a = MakeTwoWarriorBattle();
    auto b = MakeTwoWarriorBattle();

    auto ast_a = Snapshot(*a->battle);
    auto ast_b = Snapshot(*b->battle);

    const std::string msg = "ASTs of two identically-built battles differ.\nDiff: " +
                            ast_a.DiffWith(ast_b);
    EXPECT_EQ(ast_a, ast_b) << msg;
}

TEST(AstState, MutationVisibleInAst_DealDamage)
{
    auto fixture = MakeTwoWarriorBattle();
    auto before = Snapshot(*fixture->battle);

    auto players = fixture->battle->GetIfPlayers(
        [](const TPlayer* p) { return p->GetId() == 1; });
    ASSERT_FALSE(players.empty());
    TTransformator transformator(fixture->io);
    transformator.DealDamage(players[0], 1);

    auto after = Snapshot(*fixture->battle);
    EXPECT_NE(before, after);
    const std::string diff = before.DiffWith(after);
    const std::string msg = "Diff path should mention hitpoints. Got: " + diff;
    EXPECT_NE(diff.find("hitpoints"), std::string::npos) << msg;
}

TEST(AstState, MutationVisibleInAst_AddCondition)
{
    auto fixture = MakeTwoWarriorBattle();
    auto before = Snapshot(*fixture->battle);

    auto players = fixture->battle->GetIfPlayers(
        [](const TPlayer* p) { return p->GetId() == 0; });
    ASSERT_FALSE(players.empty());
    TTransformator transformator(fixture->io);
    transformator.ChangeCondition(players[0]->GetCreature(), EConditionKind::Frightened, 2);

    auto after = Snapshot(*fixture->battle);
    EXPECT_NE(before, after);
    const std::string diff = before.DiffWith(after);
    const std::string msg = "Diff: " + diff;
    EXPECT_NE(diff.find("conditions"), std::string::npos) << msg;
}

TEST(AstState, MutationVisibleInAst_Resource)
{
    auto fixture = MakeTwoWarriorBattle();
    auto before = Snapshot(*fixture->battle);

    auto players = fixture->battle->GetIfPlayers(
        [](const TPlayer* p) { return p->GetId() == 0; });
    ASSERT_FALSE(players.empty());
    TTransformator transformator(fixture->io);
    transformator.AddResource(players[0]->GetCreature()->ResourceFor(EResourceKind::Action), 1);

    auto after = Snapshot(*fixture->battle);
    EXPECT_NE(before, after);
    const std::string diff = before.DiffWith(after);
    const std::string msg = "Diff: " + diff;
    EXPECT_NE(diff.find("actions"), std::string::npos) << msg;
}

TEST(AstState, RollbackRestoresIdenticalAst_DealDamage)
{
    auto fixture = MakeTwoWarriorBattle();

    TTransformator transformator(fixture->io);
    auto players = fixture->battle->GetIfPlayers(
        [](const TPlayer* p) { return p->GetId() == 1; });
    ASSERT_FALSE(players.empty());

    auto before = Snapshot(*fixture->battle);
    auto save_point = transformator.CurrentState();

    transformator.DealDamage(players[0], 5);
    auto after = Snapshot(*fixture->battle);
    EXPECT_NE(before, after);

    transformator.Undo(save_point);
    auto restored = Snapshot(*fixture->battle);
    const std::string msg = "Rollback failed to restore AST.\nDiff: " +
                            before.DiffWith(restored);
    EXPECT_EQ(before, restored) << msg;
}

TEST(AstState, RollbackRestoresIdenticalAst_MultipleTransformations)
{
    auto fixture = MakeTwoWarriorBattle();
    TTransformator transformator(fixture->io);

    auto players = fixture->battle->GetIfPlayers(
        [](const TPlayer*) { return true; });
    ASSERT_EQ(players.size(), 2);

    auto before = Snapshot(*fixture->battle);
    auto save_point = transformator.CurrentState();

    transformator.DealDamage(players[0], 3);
    transformator.DealDamage(players[1], 7);
    transformator.ChangeCondition(players[0]->GetCreature(), EConditionKind::Prone, 1);
    transformator.AddResource(players[0]->GetCreature()->ResourceFor(EResourceKind::Action), 2);

    auto mid = Snapshot(*fixture->battle);
    EXPECT_NE(before, mid);

    transformator.Undo(save_point);

    auto restored = Snapshot(*fixture->battle);
    const std::string msg = "Multi-step rollback failed.\nDiff: " +
                            before.DiffWith(restored);
    EXPECT_EQ(before, restored) << msg;
}

TEST(AstState, BypassDetected_SetPosition)
{
    auto fixture = MakeTwoWarriorBattle();
    auto before = Snapshot(*fixture->battle);

    auto players = fixture->battle->GetIfPlayers(
        [](const TPlayer* p) { return p->GetId() == 0; });
    ASSERT_FALSE(players.empty());
    TPosition original = players[0]->GetPosition();
    players[0]->SetPosition(TPosition{2, 2});

    auto after = Snapshot(*fixture->battle);
    const std::string bypass_msg =
        "TPlayer::SetPosition bypasses TTransformator but AST should still "
        "show the difference.";
    EXPECT_NE(before, after) << bypass_msg;
    const std::string diff = before.DiffWith(after);
    const std::string diff_msg =
        "Diff path should mention position or battle_map. Got: " + diff;
    EXPECT_TRUE(diff.find("position") != std::string::npos ||
                diff.find("battle_map") != std::string::npos) << diff_msg;

    players[0]->SetPosition(original);
    // TODO(rollback): SetPosition bypasses TTransformator; follow-up will
}

TEST(AstState, ContainerDeterminism_ResourceMutationOrder)
{
    auto a = MakeTwoWarriorBattle();
    auto b = MakeTwoWarriorBattle();

    auto pa = a->battle->GetIfPlayers([](const TPlayer*) { return true; });
    auto pb = b->battle->GetIfPlayers([](const TPlayer*) { return true; });
    ASSERT_EQ(pa.size(), 2);
    ASSERT_EQ(pb.size(), 2);

    TTransformator ta(a->io);
    ta.AddResource(pa[0]->GetCreature()->ResourceFor(EResourceKind::Action), 1);
    ta.AddResource(pa[0]->GetCreature()->ResourceFor(EResourceKind::Reaction), 2);

    TTransformator tb(b->io);
    tb.AddResource(pb[0]->GetCreature()->ResourceFor(EResourceKind::Reaction), 2);
    tb.AddResource(pb[0]->GetCreature()->ResourceFor(EResourceKind::Action), 1);

    auto ast_a = Snapshot(*a->battle);
    auto ast_b = Snapshot(*b->battle);
    const std::string msg =
        "Mutation order affected AST.\nDiff: " + ast_a.DiffWith(ast_b);
    EXPECT_EQ(ast_a, ast_b) << msg;
}

TEST(AstState, ContainerDeterminism_RepeatedAstOnSameBattle)
{
    auto fixture = MakeTwoWarriorBattle();
    auto ast1 = Snapshot(*fixture->battle);
    auto ast2 = Snapshot(*fixture->battle);
    const std::string msg = "Same battle, two AST runs, should be identical.";
    EXPECT_EQ(ast1, ast2) << msg;
}

TEST(AstState, NullPointerHandling)
{
    auto fixture = MakeTwoWarriorBattle();
    auto before = Snapshot(*fixture->battle);
    EXPECT_FALSE(before.PrettyPrint().empty());

    auto players = fixture->battle->GetIfPlayers([](const TPlayer*) { return true; });
    ASSERT_FALSE(players.empty());
    players[0]->Unbind();

    auto after = Snapshot(*fixture->battle);
    EXPECT_NE(before, after);
    const std::string diff = before.DiffWith(after);
    const std::string msg = "Got: " + diff;
    EXPECT_NE(diff.find("battle_map"), std::string::npos) << msg;
}

TEST(AstState, OwnershipCycleDetected)
{
    struct TCyclic {
        TAstNode GetAst(TAstContext& ctx) const
        {
            TAstNode node = TAstNode::MakeObject("TCyclic");
            ctx.Visit(this, "TCyclic");
            ctx.Visit(this, "TCyclic");
            return node;
        }
    };

    TCyclic obj;
    TAstContext ctx;
    EXPECT_THROW(obj.GetAst(ctx), std::runtime_error);
}

#include "ast_test_fixture.h"

#include <pf2e_engine/common/ast/ast_constructable.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/common/ast/ast_node.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

// Convenience: return the AST of a freshly-prepared battle.
namespace {
TAstNode Snapshot(const TBattle& battle)
{
    TAstContext ctx;
    return battle.GetAst(ctx);
}
}  // namespace

// 1. Equality — two identical battles produce identical ASTs.
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

// 2. Mutation — any state change must change the AST. Run one per kind of
// mutation that goes through TTransformator.
TEST(AstState, MutationVisibleInAst_DealDamage)
{
    auto fixture = MakeTwoWarriorBattle();
    auto before = Snapshot(*fixture->battle);

    auto players = fixture->battle->GetIfPlayers(
        [](const TPlayer* p) { return p->GetId() == 1; });
    ASSERT_FALSE(players.empty());
    players[0]->GetCreature()->Hitpoints().ReduceHp(1);

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
    players[0]->GetCreature()->Set(ECondition::Frightened, 2);

    auto after = Snapshot(*fixture->battle);
    EXPECT_NE(before, after);
    const std::string diff = before.DiffWith(after);
    const std::string msg = "Diff: " + diff;
    EXPECT_NE(diff.find("conditions"), std::string::npos) << msg;
}

TEST(AstState, MutationVisibleInAst_ResourcePool)
{
    auto fixture = MakeTwoWarriorBattle();
    auto before = Snapshot(*fixture->battle);

    auto rid = TResourceIdManager::Instance().Register("test_resource");
    auto players = fixture->battle->GetIfPlayers(
        [](const TPlayer* p) { return p->GetId() == 0; });
    ASSERT_FALSE(players.empty());
    players[0]->GetCreature()->Resources().Add(rid, 1);

    auto after = Snapshot(*fixture->battle);
    EXPECT_NE(before, after);
    const std::string diff = before.DiffWith(after);
    const std::string msg = "Diff: " + diff;
    EXPECT_NE(diff.find("resources"), std::string::npos) << msg;
}

// 3. Rollback — save → mutate via transformator → rollback → AST identical.
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
    auto rid = TResourceIdManager::Instance().Register("test_resource_2");

    auto before = Snapshot(*fixture->battle);
    auto save_point = transformator.CurrentState();

    transformator.DealDamage(players[0], 3);
    transformator.DealDamage(players[1], 7);
    transformator.ChangeCondition(players[0]->GetCreature(), ECondition::Prone, 1);
    transformator.AddResource(&players[0]->GetCreature()->Resources(), rid, 2);

    auto mid = Snapshot(*fixture->battle);
    EXPECT_NE(before, mid);

    transformator.Undo(save_point);

    auto restored = Snapshot(*fixture->battle);
    const std::string msg = "Multi-step rollback failed.\nDiff: " +
                            before.DiffWith(restored);
    EXPECT_EQ(before, restored) << msg;
}

// 4. Direct-mutation bypass — TPlayer::SetPosition mutates state WITHOUT
// going through TTransformator. The AST catches it; this test documents the
// bypass. Follow-up work introduces TChangePosition.
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
    // SetPosition mutates two things: the player's position_ field AND the
    // battle map's cells. DiffWith reports the first diverging path; either
    // is acceptable evidence that the bypass was caught.
    const std::string diff = before.DiffWith(after);
    const std::string diff_msg =
        "Diff path should mention position or battle_map. Got: " + diff;
    EXPECT_TRUE(diff.find("position") != std::string::npos ||
                diff.find("battle_map") != std::string::npos) << diff_msg;

    // Restore so the destructor doesn't leave the map in a weird state.
    players[0]->SetPosition(original);
    // TODO(rollback): SetPosition bypasses TTransformator; follow-up will
    // route position changes through a new TChangePosition transformation
    // so that rollback restores them.
}

// 5. Container determinism.
TEST(AstState, ContainerDeterminism_ResourceInsertionOrder)
{
    auto a = MakeTwoWarriorBattle();
    auto b = MakeTwoWarriorBattle();

    auto pa = a->battle->GetIfPlayers([](const TPlayer*) { return true; });
    auto pb = b->battle->GetIfPlayers([](const TPlayer*) { return true; });
    ASSERT_EQ(pa.size(), 2);
    ASSERT_EQ(pb.size(), 2);

    auto rid_x = TResourceIdManager::Instance().Register("rid_x");
    auto rid_y = TResourceIdManager::Instance().Register("rid_y");
    auto rid_z = TResourceIdManager::Instance().Register("rid_z");

    pa[0]->GetCreature()->Resources().Add(rid_x, 1);
    pa[0]->GetCreature()->Resources().Add(rid_y, 2);
    pa[0]->GetCreature()->Resources().Add(rid_z, 3);

    pb[0]->GetCreature()->Resources().Add(rid_z, 3);
    pb[0]->GetCreature()->Resources().Add(rid_y, 2);
    pb[0]->GetCreature()->Resources().Add(rid_x, 1);

    auto ast_a = Snapshot(*a->battle);
    auto ast_b = Snapshot(*b->battle);
    const std::string msg =
        "Insertion order affected AST (unordered_map iteration leaked).\nDiff: " +
        ast_a.DiffWith(ast_b);
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

// 6. Null-pointer handling.
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

// 7. Ownership cycle — a malicious GetAst that recurses into itself must
// throw via TAstContext::Visit.
TEST(AstState, OwnershipCycleDetected)
{
    struct TCyclic {
        TAstNode GetAst(TAstContext& ctx) const
        {
            TAstNode node = TAstNode::MakeObject("TCyclic");
            ctx.Visit(this, "TCyclic");
            ctx.Visit(this, "TCyclic");  // throws here
            return node;
        }
    };

    TCyclic obj;
    TAstContext ctx;
    EXPECT_THROW(obj.GetAst(ctx), std::runtime_error);
}

#include <gtest/gtest.h>

#include <pf2e_engine/battle.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/player.h>
#include <cpp_config.h>

#include "../test_lib/mock_dice_roller.h"
#include "../test_lib/mock_interaction_system.h"

#include <filesystem>

const std::filesystem::path kPathToData{kRootDirPath + "/pf2e_engine/data"};

class ActionCombatTest : public ::testing::Test {
protected:
    using FsPath = std::filesystem::path;
    using FsDirEntry = std::filesystem::directory_entry;
    using FsRecursiveIterator = std::filesystem::recursive_directory_iterator;

    void SetUp() override {
        for (const FsDirEntry& dir_entry : FsRecursiveIterator(kPathToData)) {
            if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".json") {
                factory_.AddSource(dir_entry.path());
            }
        }
    }

    TGameObjectFactory factory_;
    TMockRng mock_rng_;
    TMockInteractionSystem mock_interaction_;
};

TEST_F(ActionCombatTest, TwoWarriorsAttackEachOther) {
    // Create warrior creatures
    auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
    TCreature warrior1 = factory_.Create<TCreature>(warrior_id);
    TCreature warrior2 = factory_.Create<TCreature>(warrior_id);

    // Create players
    TPlayer player1(&warrior1, TPlayerTeam{0}, TPlayerId{0}, "Warrior 1", "");
    TPlayer player2(&warrior2, TPlayerTeam{1}, TPlayerId{1}, "Warrior 2", "");

    // Create a simple battle map
    auto battle_map_id = TGameObjectIdManager::Instance().Register("simple_battle_map");
    TBattleMap battle_map = factory_.Create<TBattleMap>(battle_map_id);

    // Create battle
    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);
    
    // Add players to battle at different positions
    mock_rng_.ExpectCall(20, 0);
    battle.AddPlayer(std::move(player1), TPosition{0, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(player2), TPosition{1, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    // Set up expectations for the first turn
    // Player 1's turn: choose action, choose target, roll attack

    // Choose action: attack_with_weapon
    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");

    // Choose target: other player
    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");

    // Roll attack: d20 roll
    // Let's say we roll 10 on the d20. Attack bonus +9, AC 14. Leads normal hit, not critical
    mock_rng_.ExpectCall(20, 10);

    // Roll damage: longsword is 1d8 + strength modifier
    // Warrior has 18 strength (+4 modifier)
    // Let's roll 5 on the d6
    mock_rng_.ExpectCall(6, 5);

    // Verify hitpoints after the attack
    mock_interaction_.AddCheckCallback([&battle]() {
        auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
        ASSERT_FALSE(players.empty());
        // Initial HP: 21, Damage: 5 + 4 = 9, Expected HP: 12
        EXPECT_EQ(players[0]->GetCreature()->Hitpoints().GetCurrentHp(), 12);
    });

    // Start the battle
    EXPECT_THROW(battle.StartBattle(), TTooManyCallsError);

    // Verify that all expected interactions and dice rolls were made
    mock_rng_.Verify();
    mock_interaction_.Verify();

    // Check that player 2 took damage
    // Initial HP: warrior level 1 has 8 (race) + 10 + constitution mod (3) = 21 HP
    // Damage: 5 (d6) + 4 (str mod) = 9 damage
    // Expected HP: 21 - 9 = 12
    auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
    ASSERT_FALSE(players.empty());
    EXPECT_EQ(players[0]->GetCreature()->Hitpoints().GetCurrentHp(), 12);
}

TEST_F(ActionCombatTest, AttackMiss) {
    // Create warrior creatures
    auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
    TCreature warrior1 = factory_.Create<TCreature>(warrior_id);
    TCreature warrior2 = factory_.Create<TCreature>(warrior_id);

    // Create players
    TPlayer player1(&warrior1, TPlayerTeam{0}, TPlayerId{0}, "Warrior 1", "");
    TPlayer player2(&warrior2, TPlayerTeam{1}, TPlayerId{1}, "Warrior 2", "");

    // Create a simple battle map
    auto battle_map_id = TGameObjectIdManager::Instance().Register("simple_battle_map");
    TBattleMap battle_map = factory_.Create<TBattleMap>(battle_map_id);

    // Create battle
    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);

    // Add players to battle
    mock_rng_.ExpectCall(20, 0);
    battle.AddPlayer(std::move(player1), TPosition{0, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(player2), TPosition{1, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    // Player 1's turn: choose action, choose target, roll attack (miss)
    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");

    // Roll attack: d20 roll of 1
    // Attack bonus +9, AC 14. Roll 1 + 9 = 10 < 14, so it's a failure
    // Natural 1 would decrease success level further to critical failure
    mock_rng_.ExpectCall(20, 1);

    // No damage roll since the attack missed

    // Verify hitpoints after the missed attack (should still be 21)
    mock_interaction_.AddCheckCallback([&battle]() {
        auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
        ASSERT_FALSE(players.empty());
        EXPECT_EQ(players[0]->GetCreature()->Hitpoints().GetCurrentHp(), 21);
    });

    // Start the battle (will throw when mock runs out of expected interactions)
    EXPECT_THROW(battle.StartBattle(), TTooManyCallsError);

    // Verify that all expected interactions and dice rolls were made
    mock_rng_.Verify();
    mock_interaction_.Verify();
}

TEST_F(ActionCombatTest, AttackCriticalHit) {
    // Create warrior creatures
    auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
    TCreature warrior1 = factory_.Create<TCreature>(warrior_id);
    TCreature warrior2 = factory_.Create<TCreature>(warrior_id);

    // Create players
    TPlayer player1(&warrior1, TPlayerTeam{0}, TPlayerId{0}, "Warrior 1", "");
    TPlayer player2(&warrior2, TPlayerTeam{1}, TPlayerId{1}, "Warrior 2", "");

    // Create a simple battle map
    auto battle_map_id = TGameObjectIdManager::Instance().Register("simple_battle_map");
    TBattleMap battle_map = factory_.Create<TBattleMap>(battle_map_id);

    // Create battle
    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);

    // Add players to battle
    mock_rng_.ExpectCall(20, 0);
    battle.AddPlayer(std::move(player1), TPosition{0, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(player2), TPosition{1, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    // Player 1's turn: choose action, choose target, roll attack (critical hit)
    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");

    // Roll attack: natural 20 on d20 - automatic critical success
    mock_rng_.ExpectCall(20, 20);

    // Roll critical damage: longsword is (1d6 + strength modifier) * 2
    // Roll 1d6, let's say 5, then (5 + 4) * 2 = 18
    mock_rng_.ExpectCall(6, 5);

    // Verify hitpoints after the critical hit
    mock_interaction_.AddCheckCallback([&battle]() {
        auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
        ASSERT_FALSE(players.empty());
        // Initial HP: 21, Critical damage: (5 + 4) * 2 = 18, Expected HP: 3
        EXPECT_EQ(players[0]->GetCreature()->Hitpoints().GetCurrentHp(), 3);
    });

    // Start the battle (will throw when mock runs out of expected interactions)
    EXPECT_THROW(battle.StartBattle(), TTooManyCallsError);

    // Verify that all expected interactions and dice rolls were made
    mock_rng_.Verify();
    mock_interaction_.Verify();
}

TEST_F(ActionCombatTest, WarriorKillsOtherBattleEnds) {
    // Create warrior creatures
    auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
    TCreature warrior1 = factory_.Create<TCreature>(warrior_id);
    TCreature warrior2 = factory_.Create<TCreature>(warrior_id);

    // Create players
    TPlayer player1(&warrior1, TPlayerTeam{0}, TPlayerId{0}, "Warrior 1", "");
    TPlayer player2(&warrior2, TPlayerTeam{1}, TPlayerId{1}, "Warrior 2", "");

    // Create a simple battle map
    auto battle_map_id = TGameObjectIdManager::Instance().Register("simple_battle_map");
    TBattleMap battle_map = factory_.Create<TBattleMap>(battle_map_id);

    // Create battle
    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);

    // Add players to battle
    mock_rng_.ExpectCall(20, 0);
    battle.AddPlayer(std::move(player1), TPosition{0, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(player2), TPosition{1, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    // Warrior 1 attacks Warrior 2 three times to kill them
    // Warrior HP: 21, each hit deals around 5-10 damage

    // First attack - hit
    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");
    mock_rng_.ExpectCall(20, 10);  // Hit (10 + 9 = 19 >= 14 AC)
    mock_rng_.ExpectCall(6, 6);    // Max damage: 6 + 4 = 10

    // Verify hitpoints after first attack: 21 - 10 = 11
    mock_interaction_.AddCheckCallback([&battle]() {
        auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
        ASSERT_FALSE(players.empty());
        EXPECT_EQ(players[0]->GetCreature()->Hitpoints().GetCurrentHp(), 11);
    });

    // Second attack - hit (with -5 MAP)
    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");
    mock_rng_.ExpectCall(20, 15);  // Hit (15 + 9 - 5 = 19 >= 14 AC)
    mock_rng_.ExpectCall(6, 6);    // Max damage: 6 + 4 = 10

    // Verify hitpoints after second attack: 11 - 10 = 1
    mock_interaction_.AddCheckCallback([&battle]() {
        auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
        ASSERT_FALSE(players.empty());
        EXPECT_EQ(players[0]->GetCreature()->Hitpoints().GetCurrentHp(), 1);
    });

    // Third attack - critical hit to finish them off (with -10 MAP)
    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");
    mock_rng_.ExpectCall(20, 20);  // Natural 20 - critical hit
    mock_rng_.ExpectCall(6, 1);    // Crit damage: (1 + 4) * 2 = 10

    // Battle should end naturally when Warrior 2 dies (no throw expected)
    battle.StartBattle();

    // Verify that all expected interactions and dice rolls were made
    mock_rng_.Verify();
    mock_interaction_.Verify();

    // Check that player 2 is dead
    auto players2 = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
    ASSERT_FALSE(players2.empty());
    EXPECT_FALSE(players2[0]->GetCreature()->IsAlive());

    // Check that player 1 is still alive
    auto players1 = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 0; });
    ASSERT_FALSE(players1.empty());
    EXPECT_TRUE(players1[0]->GetCreature()->IsAlive());
}

TEST_F(ActionCombatTest, MultipleAttackPenalty) {
    // Create warrior creatures
    auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
    TCreature warrior1 = factory_.Create<TCreature>(warrior_id);
    TCreature warrior2 = factory_.Create<TCreature>(warrior_id);

    // Create players
    TPlayer player1(&warrior1, TPlayerTeam{0}, TPlayerId{0}, "Warrior 1", "");
    TPlayer player2(&warrior2, TPlayerTeam{1}, TPlayerId{1}, "Warrior 2", "");

    // Create a simple battle map
    auto battle_map_id = TGameObjectIdManager::Instance().Register("simple_battle_map");
    TBattleMap battle_map = factory_.Create<TBattleMap>(battle_map_id);

    // Create battle
    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);

    // Add players to battle
    mock_rng_.ExpectCall(20, 0);
    battle.AddPlayer(std::move(player1), TPosition{0, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(player2), TPosition{1, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    // Test Multiple Attack Penalty (MAP)
    // Attack bonus: +9 (STR +4, proficiency +5)
    // AC: 14 (10 + DEX +1 + proficiency +3)
    //
    // First attack: no penalty, roll 5 + 9 = 14 >= 14 AC -> HIT
    // Second attack: -5 MAP, roll 5 + 9 - 5 = 9 < 14 AC -> MISS
    //
    // Both attacks use the same d20 roll (5) to demonstrate MAP effect

    // First attack - should hit (5 + 9 = 14 >= 14)
    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");
    mock_rng_.ExpectCall(20, 5);   // d20 roll = 5, total = 5 + 9 = 14 (hit)
    mock_rng_.ExpectCall(6, 3);    // Damage: 3 + 4 = 7

    // Verify hitpoints after first attack: 21 - 7 = 14
    mock_interaction_.AddCheckCallback([&battle]() {
        auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
        ASSERT_FALSE(players.empty());
        EXPECT_EQ(players[0]->GetCreature()->Hitpoints().GetCurrentHp(), 14);
    });

    // Second attack - should miss due to MAP (5 + 9 - 5 = 9 < 14)
    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");
    mock_rng_.ExpectCall(20, 5);   // Same d20 roll = 5, but with -5 MAP: 5 + 9 - 5 = 9 (miss)
    // No damage roll since attack missed

    // Verify hitpoints after missed attack (still 14)
    mock_interaction_.AddCheckCallback([&battle]() {
        auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
        ASSERT_FALSE(players.empty());
        EXPECT_EQ(players[0]->GetCreature()->Hitpoints().GetCurrentHp(), 14);
    });

    // Start the battle (will throw when mock runs out of expected interactions)
    EXPECT_THROW(battle.StartBattle(), TTooManyCallsError);

    // Verify that all expected interactions and dice rolls were made
    mock_rng_.Verify();
    mock_interaction_.Verify();

    // Check that player 2 took damage only from the first attack
    // Initial HP: 21
    // Damage from first attack: 3 + 4 = 7
    // Second attack missed due to MAP
    // Expected HP: 21 - 7 = 14
    auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
    ASSERT_FALSE(players.empty());
    EXPECT_EQ(players[0]->GetCreature()->Hitpoints().GetCurrentHp(), 14);
}

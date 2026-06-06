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
    auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
    TCreature warrior1 = factory_.Create<TCreature>(warrior_id);
    TCreature warrior2 = factory_.Create<TCreature>(warrior_id);

    TPlayer player1(&warrior1, TPlayerTeam{0}, TPlayerId{0}, "Warrior 1", "");
    TPlayer player2(&warrior2, TPlayerTeam{1}, TPlayerId{1}, "Warrior 2", "");

    auto battle_map_id = TGameObjectIdManager::Instance().Register("simple_battle_map");
    TBattleMap battle_map = factory_.Create<TBattleMap>(battle_map_id);

    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);
    
    mock_rng_.ExpectCall(20, 0);
    battle.AddPlayer(std::move(player1), TPosition{0, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(player2), TPosition{1, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);


    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");

    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");

    mock_rng_.ExpectCall(20, 10);

    mock_rng_.ExpectCall(6, 5);

    mock_interaction_.AddCheckCallback([&battle]() {
        auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
        ASSERT_FALSE(players.empty());
        EXPECT_EQ(players[0]->GetCreature()->Hitpoints()->GetCurrentHp(), 12);
    });

    EXPECT_THROW(battle.StartBattle(), TTooManyCallsError);

    mock_rng_.Verify();
    mock_interaction_.Verify();

    auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
    ASSERT_FALSE(players.empty());
    EXPECT_EQ(players[0]->GetCreature()->Hitpoints()->GetCurrentHp(), 12);
}

TEST_F(ActionCombatTest, AttackMiss) {
    auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
    TCreature warrior1 = factory_.Create<TCreature>(warrior_id);
    TCreature warrior2 = factory_.Create<TCreature>(warrior_id);

    TPlayer player1(&warrior1, TPlayerTeam{0}, TPlayerId{0}, "Warrior 1", "");
    TPlayer player2(&warrior2, TPlayerTeam{1}, TPlayerId{1}, "Warrior 2", "");

    auto battle_map_id = TGameObjectIdManager::Instance().Register("simple_battle_map");
    TBattleMap battle_map = factory_.Create<TBattleMap>(battle_map_id);

    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);

    mock_rng_.ExpectCall(20, 0);
    battle.AddPlayer(std::move(player1), TPosition{0, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(player2), TPosition{1, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");

    mock_rng_.ExpectCall(20, 1);


    mock_interaction_.AddCheckCallback([&battle]() {
        auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
        ASSERT_FALSE(players.empty());
        EXPECT_EQ(players[0]->GetCreature()->Hitpoints()->GetCurrentHp(), 21);
    });

    EXPECT_THROW(battle.StartBattle(), TTooManyCallsError);

    mock_rng_.Verify();
    mock_interaction_.Verify();
}

TEST_F(ActionCombatTest, AttackCriticalHit) {
    auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
    TCreature warrior1 = factory_.Create<TCreature>(warrior_id);
    TCreature warrior2 = factory_.Create<TCreature>(warrior_id);

    TPlayer player1(&warrior1, TPlayerTeam{0}, TPlayerId{0}, "Warrior 1", "");
    TPlayer player2(&warrior2, TPlayerTeam{1}, TPlayerId{1}, "Warrior 2", "");

    auto battle_map_id = TGameObjectIdManager::Instance().Register("simple_battle_map");
    TBattleMap battle_map = factory_.Create<TBattleMap>(battle_map_id);

    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);

    mock_rng_.ExpectCall(20, 0);
    battle.AddPlayer(std::move(player1), TPosition{0, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(player2), TPosition{1, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");

    mock_rng_.ExpectCall(20, 20);

    mock_rng_.ExpectCall(6, 5);

    mock_interaction_.AddCheckCallback([&battle]() {
        auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
        ASSERT_FALSE(players.empty());
        EXPECT_EQ(players[0]->GetCreature()->Hitpoints()->GetCurrentHp(), 3);
    });

    EXPECT_THROW(battle.StartBattle(), TTooManyCallsError);

    mock_rng_.Verify();
    mock_interaction_.Verify();
}

TEST_F(ActionCombatTest, WarriorKillsOtherBattleEnds) {
    auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
    TCreature warrior1 = factory_.Create<TCreature>(warrior_id);
    TCreature warrior2 = factory_.Create<TCreature>(warrior_id);

    TPlayer player1(&warrior1, TPlayerTeam{0}, TPlayerId{0}, "Warrior 1", "");
    TPlayer player2(&warrior2, TPlayerTeam{1}, TPlayerId{1}, "Warrior 2", "");

    auto battle_map_id = TGameObjectIdManager::Instance().Register("simple_battle_map");
    TBattleMap battle_map = factory_.Create<TBattleMap>(battle_map_id);

    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);

    mock_rng_.ExpectCall(20, 0);
    battle.AddPlayer(std::move(player1), TPosition{0, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(player2), TPosition{1, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);


    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");
    mock_rng_.ExpectCall(20, 10);
    mock_rng_.ExpectCall(6, 6);

    mock_interaction_.AddCheckCallback([&battle]() {
        auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
        ASSERT_FALSE(players.empty());
        EXPECT_EQ(players[0]->GetCreature()->Hitpoints()->GetCurrentHp(), 11);
    });

    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");
    mock_rng_.ExpectCall(20, 15);
    mock_rng_.ExpectCall(6, 6);

    mock_interaction_.AddCheckCallback([&battle]() {
        auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
        ASSERT_FALSE(players.empty());
        EXPECT_EQ(players[0]->GetCreature()->Hitpoints()->GetCurrentHp(), 1);
    });

    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");
    mock_rng_.ExpectCall(20, 20);
    mock_rng_.ExpectCall(6, 1);

    battle.StartBattle();

    mock_rng_.Verify();
    mock_interaction_.Verify();

    auto players2 = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
    ASSERT_FALSE(players2.empty());
    EXPECT_FALSE(players2[0]->GetCreature()->IsAlive());

    auto players1 = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 0; });
    ASSERT_FALSE(players1.empty());
    EXPECT_TRUE(players1[0]->GetCreature()->IsAlive());
}

TEST_F(ActionCombatTest, MultipleAttackPenalty) {
    auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
    TCreature warrior1 = factory_.Create<TCreature>(warrior_id);
    TCreature warrior2 = factory_.Create<TCreature>(warrior_id);

    TPlayer player1(&warrior1, TPlayerTeam{0}, TPlayerId{0}, "Warrior 1", "");
    TPlayer player2(&warrior2, TPlayerTeam{1}, TPlayerId{1}, "Warrior 2", "");

    auto battle_map_id = TGameObjectIdManager::Instance().Register("simple_battle_map");
    TBattleMap battle_map = factory_.Create<TBattleMap>(battle_map_id);

    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);

    mock_rng_.ExpectCall(20, 0);
    battle.AddPlayer(std::move(player1), TPosition{0, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(player2), TPosition{1, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);


    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");
    mock_rng_.ExpectCall(20, 5);
    mock_rng_.ExpectCall(6, 3);

    mock_interaction_.AddCheckCallback([&battle]() {
        auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
        ASSERT_FALSE(players.empty());
        EXPECT_EQ(players[0]->GetCreature()->Hitpoints()->GetCurrentHp(), 14);
    });

    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior 2");
    mock_rng_.ExpectCall(20, 5);

    mock_interaction_.AddCheckCallback([&battle]() {
        auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
        ASSERT_FALSE(players.empty());
        EXPECT_EQ(players[0]->GetCreature()->Hitpoints()->GetCurrentHp(), 14);
    });

    EXPECT_THROW(battle.StartBattle(), TTooManyCallsError);

    mock_rng_.Verify();
    mock_interaction_.Verify();

    auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
    ASSERT_FALSE(players.empty());
    EXPECT_EQ(players[0]->GetCreature()->Hitpoints()->GetCurrentHp(), 14);
}

TEST_F(ActionCombatTest, WizardCastsFireball) {
    auto wizard_id = TGameObjectIdManager::Instance().Register("wizard");
    auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
    TCreature wizard = factory_.Create<TCreature>(wizard_id);
    TCreature warrior = factory_.Create<TCreature>(warrior_id);

    TPlayer player1(&wizard, TPlayerTeam{0}, TPlayerId{0}, "Wizard", "");
    TPlayer player2(&warrior, TPlayerTeam{1}, TPlayerId{1}, "Warrior", "");

    auto battle_map_id = TGameObjectIdManager::Instance().Register("simple_battle_map");
    TBattleMap battle_map = factory_.Create<TBattleMap>(battle_map_id);

    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);

    mock_rng_.ExpectCall(20, 0);
    battle.AddPlayer(std::move(player1), TPosition{0, 0});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(player2), TPosition{5, 5});
    EXPECT_EQ(mock_rng_.RemainingCalls(), 0);

    mock_interaction_.ExpectChoice(0, "next action", "fireball");

    mock_interaction_.ExpectChoice(0, "burst center", "5 5");


    mock_rng_.ExpectCall(6, 3);
    mock_rng_.ExpectCall(6, 4);
    mock_rng_.ExpectCall(6, 3);
    mock_rng_.ExpectCall(6, 5);
    mock_rng_.ExpectCall(6, 2);
    mock_rng_.ExpectCall(6, 4);

    mock_interaction_.AddCheckCallback([&battle]() {
        auto players = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
        ASSERT_FALSE(players.empty());
        EXPECT_EQ(players[0]->GetCreature()->Hitpoints()->GetCurrentHp(), 0);
    });

    battle.StartBattle();

    mock_rng_.Verify();
    mock_interaction_.Verify();

    auto warriors = battle.GetIfPlayers([](const TPlayer* p) { return p->GetId() == 1; });
    ASSERT_FALSE(warriors.empty());
    EXPECT_FALSE(warriors[0]->GetCreature()->IsAlive());
}

TEST_F(ActionCombatTest, ReachFilterExcludesOutOfRangeTargets) {
    auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
    TCreature attacker_c = factory_.Create<TCreature>(warrior_id);
    TCreature near_c = factory_.Create<TCreature>(warrior_id);
    TCreature far_c = factory_.Create<TCreature>(warrior_id);

    TPlayer attacker(&attacker_c, TPlayerTeam{0}, TPlayerId{0}, "Attacker", "");
    TPlayer near_target(&near_c, TPlayerTeam{1}, TPlayerId{1}, "Near", "");
    TPlayer far_target(&far_c, TPlayerTeam{1}, TPlayerId{2}, "Far", "");

    auto battle_map_id = TGameObjectIdManager::Instance().Register("simple_battle_map");
    TBattleMap battle_map = factory_.Create<TBattleMap>(battle_map_id);
    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);

    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(attacker), TPosition{0, 0});
    mock_rng_.ExpectCall(20, 10);
    battle.AddPlayer(std::move(near_target), TPosition{1, 0});
    mock_rng_.ExpectCall(20, 0);
    battle.AddPlayer(std::move(far_target), TPosition{5, 0});

    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Far");

    EXPECT_THROW(battle.StartBattle(), std::logic_error);
}

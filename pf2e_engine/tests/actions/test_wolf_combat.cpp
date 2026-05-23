#include <gtest/gtest.h>

#include <pf2e_engine/battle.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/player.h>
#include <cpp_config.h>

#include "../test_lib/mock_dice_roller.h"
#include "../test_lib/mock_interaction_system.h"

#include <filesystem>

namespace {
const std::filesystem::path kPathToData{kRootDirPath + "/pf2e_engine/data"};
}

// Exercises the wolf: a natural attack (no inventory weapon -- the jaws live
// on the creature's `natural_weapons` and are picked by `choose_weapon`) plus
// the "Pack Attack" feat (+1d4 damage when an ally is adjacent to the target).
class WolfCombatTest : public ::testing::Test {
protected:
    using FsDirEntry = std::filesystem::directory_entry;
    using FsRecursiveIterator = std::filesystem::recursive_directory_iterator;

    void SetUp() override {
        for (const FsDirEntry& dir_entry : FsRecursiveIterator(kPathToData)) {
            if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".json") {
                factory_.AddSource(dir_entry.path());
            }
        }
    }

    TCreature MakeWolf() {
        return factory_.Create<TCreature>(
            TGameObjectIdManager::Instance().Register("wolf"));
    }

    TCreature MakeWarrior() {
        return factory_.Create<TCreature>(
            TGameObjectIdManager::Instance().Register("warrior"));
    }

    TBattleMap MakeMap() {
        return factory_.Create<TBattleMap>(
            TGameObjectIdManager::Instance().Register("simple_battle_map"));
    }

    int WarriorHp(TBattle& battle, int player_id) {
        auto players = battle.GetIfPlayers(
            [player_id](const TPlayer* p) { return p->GetId() == player_id; });
        EXPECT_FALSE(players.empty());
        return players[0]->GetCreature()->Hitpoints().GetCurrentHp();
    }

    TGameObjectFactory factory_;
    TMockRng mock_rng_;
    TMockInteractionSystem mock_interaction_;
};

// A lone wolf bites the warrior. The natural-attack weapon comes from the
// action's `variables` section; Pack Attack finds no adjacent ally, so it
// contributes nothing -- only the jaws d6 is rolled.
TEST_F(WolfCombatTest, NaturalAttackHitsWithoutPack) {
    TCreature wolf = MakeWolf();
    TCreature warrior = MakeWarrior();

    TPlayer wolf_player(&wolf, TPlayerTeam{0}, TPlayerId{0}, "Wolf A", "");
    TPlayer warrior_player(&warrior, TPlayerTeam{1}, TPlayerId{1}, "Warrior", "");

    TBattleMap battle_map = MakeMap();
    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);

    mock_rng_.ExpectCall(20, 0);   // wolf wins initiative
    battle.AddPlayer(std::move(wolf_player), TPosition{0, 0});
    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(warrior_player), TPosition{1, 0});

    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior");
    mock_rng_.ExpectCall(20, 15);  // attack roll -> hit
    mock_rng_.ExpectCall(6, 5);    // jaws damage d6 (no Pack Attack d4)

    // Warrior HP 21, damage 5 + 2 (wolf Str mod) = 7 -> 14.
    mock_interaction_.AddCheckCallback([&]() {
        EXPECT_EQ(WarriorHp(battle, 1), 14);
    });

    EXPECT_THROW(battle.StartBattle(), TTooManyCallsError);
    mock_rng_.Verify();
    mock_interaction_.Verify();

    EXPECT_EQ(WarriorHp(battle, 1), 14);
}

// Three wolves surround the warrior. Wolf A bites; Pack Attack sees both
// allies (Wolf B and Wolf C) within reach of the target and contributes +1d4.
TEST_F(WolfCombatTest, PackAttackAddsBonusDamage) {
    TCreature wolf_a = MakeWolf();
    TCreature wolf_b = MakeWolf();
    TCreature wolf_c = MakeWolf();
    TCreature warrior = MakeWarrior();

    TPlayer wolf_a_player(&wolf_a, TPlayerTeam{0}, TPlayerId{0}, "Wolf A", "");
    TPlayer wolf_b_player(&wolf_b, TPlayerTeam{0}, TPlayerId{1}, "Wolf B", "");
    TPlayer wolf_c_player(&wolf_c, TPlayerTeam{0}, TPlayerId{2}, "Wolf C", "");
    TPlayer warrior_player(&warrior, TPlayerTeam{1}, TPlayerId{3}, "Warrior", "");

    TBattleMap battle_map = MakeMap();
    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);

    mock_rng_.ExpectCall(20, 0);   // Wolf A acts first
    battle.AddPlayer(std::move(wolf_a_player), TPosition{0, 0});
    mock_rng_.ExpectCall(20, 10);
    battle.AddPlayer(std::move(wolf_b_player), TPosition{2, 0});
    mock_rng_.ExpectCall(20, 15);
    battle.AddPlayer(std::move(wolf_c_player), TPosition{1, 1});
    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(warrior_player), TPosition{1, 0});

    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior");
    mock_rng_.ExpectCall(20, 15);  // attack roll -> hit
    mock_rng_.ExpectCall(6, 5);    // jaws damage d6
    mock_rng_.ExpectCall(4, 3);    // Pack Attack bonus d4

    // Warrior HP 21, damage (5 + 2) + 3 = 10 -> 11.
    mock_interaction_.AddCheckCallback([&]() {
        EXPECT_EQ(WarriorHp(battle, 3), 11);
    });

    EXPECT_THROW(battle.StartBattle(), TTooManyCallsError);
    mock_rng_.Verify();
    mock_interaction_.Verify();

    EXPECT_EQ(WarriorHp(battle, 3), 11);
}

// Only Wolf B is in reach of the target -- Wolf C is far. One adjacent ally
// is below Pack Attack's threshold of two, so no d4 is rolled.
TEST_F(WolfCombatTest, PackAttackGatedByAllyCount) {
    TCreature wolf_a = MakeWolf();
    TCreature wolf_b = MakeWolf();
    TCreature wolf_c = MakeWolf();
    TCreature warrior = MakeWarrior();

    TPlayer wolf_a_player(&wolf_a, TPlayerTeam{0}, TPlayerId{0}, "Wolf A", "");
    TPlayer wolf_b_player(&wolf_b, TPlayerTeam{0}, TPlayerId{1}, "Wolf B", "");
    TPlayer wolf_c_player(&wolf_c, TPlayerTeam{0}, TPlayerId{2}, "Wolf C", "");
    TPlayer warrior_player(&warrior, TPlayerTeam{1}, TPlayerId{3}, "Warrior", "");

    TBattleMap battle_map = MakeMap();
    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);

    mock_rng_.ExpectCall(20, 0);
    battle.AddPlayer(std::move(wolf_a_player), TPosition{0, 0});
    mock_rng_.ExpectCall(20, 10);
    battle.AddPlayer(std::move(wolf_b_player), TPosition{2, 0});      // in reach
    mock_rng_.ExpectCall(20, 15);
    battle.AddPlayer(std::move(wolf_c_player), TPosition{5, 5});      // far away
    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(warrior_player), TPosition{1, 0});

    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior");
    mock_rng_.ExpectCall(20, 15);  // attack roll -> hit
    mock_rng_.ExpectCall(6, 5);    // jaws damage d6 only -- no d4

    // Warrior HP 21, damage 5 + 2 = 7 -> 14.
    mock_interaction_.AddCheckCallback([&]() {
        EXPECT_EQ(WarriorHp(battle, 3), 14);
    });

    EXPECT_THROW(battle.StartBattle(), TTooManyCallsError);
    mock_rng_.Verify();
    mock_interaction_.Verify();

    EXPECT_EQ(WarriorHp(battle, 3), 14);
}

// On a critical hit the Pack Attack d4 doubles along with the weapon damage.
TEST_F(WolfCombatTest, PackAttackCriticalDoublesBonus) {
    TCreature wolf_a = MakeWolf();
    TCreature wolf_b = MakeWolf();
    TCreature wolf_c = MakeWolf();
    TCreature warrior = MakeWarrior();

    TPlayer wolf_a_player(&wolf_a, TPlayerTeam{0}, TPlayerId{0}, "Wolf A", "");
    TPlayer wolf_b_player(&wolf_b, TPlayerTeam{0}, TPlayerId{1}, "Wolf B", "");
    TPlayer wolf_c_player(&wolf_c, TPlayerTeam{0}, TPlayerId{2}, "Wolf C", "");
    TPlayer warrior_player(&warrior, TPlayerTeam{1}, TPlayerId{3}, "Warrior", "");

    TBattleMap battle_map = MakeMap();
    TBattle battle(std::move(battle_map), &mock_rng_, mock_interaction_);

    mock_rng_.ExpectCall(20, 0);
    battle.AddPlayer(std::move(wolf_a_player), TPosition{0, 0});
    mock_rng_.ExpectCall(20, 10);
    battle.AddPlayer(std::move(wolf_b_player), TPosition{2, 0});
    mock_rng_.ExpectCall(20, 15);
    battle.AddPlayer(std::move(wolf_c_player), TPosition{1, 1});
    mock_rng_.ExpectCall(20, 20);
    battle.AddPlayer(std::move(warrior_player), TPosition{1, 0});

    mock_interaction_.ExpectChoice(0, "next action", "attack_with_weapon");
    mock_interaction_.ExpectChoice(0, "target", "Warrior");
    mock_rng_.ExpectCall(20, 20);  // natural 20 -> critical hit
    mock_rng_.ExpectCall(6, 5);    // jaws damage d6
    mock_rng_.ExpectCall(4, 3);    // Pack Attack bonus d4

    // Warrior HP 21, crit damage ((5 + 2) + 3) * 2 = 20 -> 1.
    mock_interaction_.AddCheckCallback([&]() {
        EXPECT_EQ(WarriorHp(battle, 3), 1);
    });

    EXPECT_THROW(battle.StartBattle(), TTooManyCallsError);
    mock_rng_.Verify();
    mock_interaction_.Verify();

    EXPECT_EQ(WarriorHp(battle, 3), 1);
}

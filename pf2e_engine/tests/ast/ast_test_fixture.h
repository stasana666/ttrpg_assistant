#pragma once

#include <pf2e_engine/battle.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/player.h>
#include <cpp_config.h>

#include "../test_lib/mock_dice_roller.h"
#include "../test_lib/mock_interaction_system.h"

#include <algorithm>
#include <filesystem>
#include <vector>

// Helper that lifts ActionCombatTest::SetUp into a reusable form. The factory
// is loaded with every .json under pf2e_engine/data. Directory entries are
// sorted before being fed to the factory so that two runs (or two instances)
// see the same registration order — important for determinism of any
// allocator-order-dependent state.
inline void LoadFactory(TGameObjectFactory& factory)
{
    const std::filesystem::path path{kRootDirPath + "/pf2e_engine/data"};

    std::vector<std::filesystem::path> json_files;
    for (const auto& dir_entry :
         std::filesystem::recursive_directory_iterator(path)) {
        if (dir_entry.is_regular_file() &&
            dir_entry.path().extension() == ".json") {
            json_files.push_back(dir_entry.path());
        }
    }
    std::sort(json_files.begin(), json_files.end());
    for (const auto& p : json_files) {
        factory.AddSource(p);
    }
}

// Two-warrior battle. Returns a battle, two creatures, and two players ready
// to be inspected. The caller owns `creatures` (TPlayer holds non-owning
// TCreature*); the deque ensures stable addresses across pushes.
struct TTestBattle {
    TGameObjectFactory factory;
    TMockRng rng;
    TMockInteractionSystem io;
    std::deque<TCreature> creatures;
    std::unique_ptr<TBattle> battle;
};

inline std::unique_ptr<TTestBattle> MakeTwoWarriorBattle()
{
    auto fixture = std::make_unique<TTestBattle>();
    LoadFactory(fixture->factory);

    auto warrior_id = TGameObjectIdManager::Instance().Register("warrior");
    fixture->creatures.push_back(fixture->factory.Create<TCreature>(warrior_id));
    fixture->creatures.push_back(fixture->factory.Create<TCreature>(warrior_id));

    TPlayer p1(&fixture->creatures[0], TPlayerTeam{0}, TPlayerId{0}, "Warrior 1", "");
    TPlayer p2(&fixture->creatures[1], TPlayerTeam{1}, TPlayerId{1}, "Warrior 2", "");

    auto map_id = TGameObjectIdManager::Instance().Register("simple_battle_map");
    TBattleMap map = fixture->factory.Create<TBattleMap>(map_id);

    fixture->battle = std::make_unique<TBattle>(
        std::move(map), &fixture->rng, fixture->io);

    // Initiative rolls happen on AddPlayer; pre-seed deterministic results.
    fixture->rng.ExpectCall(20, 10);
    fixture->battle->AddPlayer(std::move(p1), TPosition{0, 0});
    fixture->rng.ExpectCall(20, 5);
    fixture->battle->AddPlayer(std::move(p2), TPosition{1, 0});

    return fixture;
}

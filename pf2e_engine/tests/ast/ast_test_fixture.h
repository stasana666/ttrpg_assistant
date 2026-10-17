#pragma once

#include <pf2e_engine/battle.h>
#include <pf2e_engine/rules.h>
#include <pf2e_engine/player.h>

#include "../test_lib/mock_dice_roller.h"
#include "../test_lib/mock_interaction_system.h"
#include "../test_lib/warrior.h"

#include <deque>
#include <memory>

struct TTestBattle {
    TMockRng rng;
    TMockInteractionSystem io;
    std::deque<TCreature> creatures;
    std::unique_ptr<TBattle> battle;
};

inline std::unique_ptr<TTestBattle> MakeTwoWarriorBattle()
{
    auto fixture = std::make_unique<TTestBattle>();
    fixture->creatures.push_back(MakeWarrior());
    fixture->creatures.push_back(MakeWarrior());

    TPlayer p1(&fixture->creatures[0], TPlayerTeam{0}, TPlayerId{0}, "Warrior 1", "");
    TPlayer p2(&fixture->creatures[1], TPlayerTeam{1}, TPlayerId{1}, "Warrior 2", "");

    fixture->battle = std::make_unique<TBattle>(
        TBattleMap(10, 10), &fixture->rng, fixture->io, CreateAbilityRules());

    fixture->rng.ExpectCall(20, 10);
    fixture->battle->AddPlayer(std::move(p1), TPosition{0, 0});
    fixture->rng.ExpectCall(20, 5);
    fixture->battle->AddPlayer(std::move(p2), TPosition{1, 0});

    return fixture;
}

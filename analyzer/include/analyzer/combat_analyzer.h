#pragma once

#include <pf2e_engine/game_object_logic/game_object_factory.h>

#include <cstddef>
#include <string>

struct TCombatantStats {
    std::string name;
    int team = 0;
    size_t wins = 0;
    size_t deaths = 0;
};

struct TAnalysisResult {
    size_t iterations = 0;
    size_t draws = 0;
    TCombatantStats side_a;
    TCombatantStats side_b;
};

// Runs Monte-Carlo combat simulations on top of the headless engine.
class TCombatAnalyzer {
public:
    TCombatAnalyzer();

    // Simulates `iterations` fights between two identical warriors and
    // aggregates win / death frequencies.
    TAnalysisResult RunTwoWarriors(size_t iterations);

private:
    TGameObjectFactory factory_;
};

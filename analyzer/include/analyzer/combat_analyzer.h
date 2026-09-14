#pragma once

#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/inventory/remaster_profiles.h>

#include <cstddef>
#include <string>
#include <vector>

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

struct TEncounterAnalysisResult {
    size_t iterations = 0;
    size_t draws = 0;
    std::vector<TCombatantStats> combatants;
};

class TCombatAnalyzer {
public:
    TCombatAnalyzer();

    TAnalysisResult RunTwoWarriors(size_t iterations);
    TEncounterAnalysisResult RunFighterAgainstWolves(size_t iterations,
        const THumanFighter1Options& fighter_options = {});

private:
    TGameObjectFactory factory_;
};

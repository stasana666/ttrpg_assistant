#pragma once

#include <pf2e_engine/inventory/remaster_profiles.h>
#include <pf2e_engine/rules.h>

#include <cstddef>
#include <string>
#include <vector>

struct TCombatantStats {
    std::string name;
    int team = 0;
    size_t wins = 0;
    size_t deaths = 0;
};

struct TEncounterAnalysisResult {
    size_t iterations = 0;
    size_t draws = 0;
    std::vector<TCombatantStats> combatants;
};

class TCombatAnalyzer {
public:
    TEncounterAnalysisResult RunTwoFighters(size_t iterations);
    TEncounterAnalysisResult RunFighterAgainstWolves(size_t iterations,
        const THumanFighter1Options& fighter_options = {});

private:
    struct TCombatant {
        std::string Name;
        int Team;
        TPosition Position;
        std::function<TCreature()> Create;
    };

    TEncounterAnalysisResult Run(size_t iterations, const std::vector<TCombatant>& combatants);

    std::shared_ptr<const TAbilityRules> rules_ = CreateAbilityRules();
};

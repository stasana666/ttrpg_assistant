#include <analyzer/combat_analyzer.h>

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void PrintCombatant(const TCombatantStats& stats, size_t iterations)
{
    double win_rate = iterations != 0 ? static_cast<double>(stats.wins) / iterations : 0.0;
    double death_rate = iterations != 0 ? static_cast<double>(stats.deaths) / iterations : 0.0;
    std::cout << "  " << stats.name << " (team " << stats.team << "): "
              << "win " << win_rate * 100.0 << "%  "
              << "death " << death_rate * 100.0 << "%\n";
}

}

int main(int argc, char** argv)
{
    size_t iterations = 1000;
    std::string_view scenario = "two-fighters";
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string_view(argv[i]) == "--iterations") {
            iterations = std::strtoul(argv[i + 1], nullptr, 10);
        }
        if (std::string_view(argv[i]) == "--scenario") {
            scenario = argv[i + 1];
        }
    }

    TCombatAnalyzer analyzer;
    TEncounterAnalysisResult result;
    if (scenario == "fighter-wolves") {
        result = analyzer.RunFighterAgainstWolves(iterations);
        std::cout << "Generated fighter versus two wolves over " << result.iterations << " simulations:\n";
    } else if (scenario == "two-fighters") {
        result = analyzer.RunTwoFighters(iterations);
        std::cout << "Two-fighter combat analysis over " << result.iterations << " simulations:\n";
    } else {
        std::cerr << "Unknown scenario: " << scenario << "\n";
        return 1;
    }
    for (const auto& combatant : result.combatants) {
        PrintCombatant(combatant, result.iterations);
    }
    std::cout << "  draws: " << result.draws << "\n";
    return 0;
}

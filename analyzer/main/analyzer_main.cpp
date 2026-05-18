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

}  // namespace

int main(int argc, char** argv)
{
    size_t iterations = 1000;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string_view(argv[i]) == "--iterations") {
            iterations = std::strtoul(argv[i + 1], nullptr, 10);
        }
    }

    TCombatAnalyzer analyzer;
    TAnalysisResult result = analyzer.RunTwoWarriors(iterations);

    std::cout << "Two-warrior combat analysis over " << result.iterations << " simulations:\n";
    PrintCombatant(result.side_a, result.iterations);
    PrintCombatant(result.side_b, result.iterations);
    std::cout << "  draws: " << result.draws << "\n";

    return 0;
}

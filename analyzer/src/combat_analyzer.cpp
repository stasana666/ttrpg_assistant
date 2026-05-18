#include <analyzer/combat_analyzer.h>

#include <analyzer/aggressive_melee_strategy.h>
#include <analyzer/automated_interaction_system.h>

#include <pf2e_engine/battle.h>
#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/random.h>

#include <cpp_config.h>

#include <filesystem>

namespace {

const std::filesystem::path kPathToData{kRootDirPath + "/pf2e_engine/data"};

}  // namespace

TCombatAnalyzer::TCombatAnalyzer()
{
    for (const auto& entry : std::filesystem::recursive_directory_iterator(kPathToData)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            factory_.AddSource(entry.path());
        }
    }
}

TAnalysisResult TCombatAnalyzer::RunTwoWarriors(size_t iterations)
{
    const TGameObjectId warrior_id = TGameObjectIdManager::Instance().Register("warrior");
    const TGameObjectId map_id = TGameObjectIdManager::Instance().Register("simple_battle_map");

    TAnalysisResult result;
    result.side_a = {.name = "Warrior A", .team = 1, .wins = 0, .deaths = 0};
    result.side_b = {.name = "Warrior B", .team = 2, .wins = 0, .deaths = 0};

    for (size_t i = 0; i < iterations; ++i) {
        TRandomGenerator rng(static_cast<int>(i));
        TAggressiveMeleeStrategy strategy;
        TAutomatedInteractionSystem io_system(strategy);

        TCreature creature_a = factory_.Create<TCreature>(warrior_id);
        TCreature creature_b = factory_.Create<TCreature>(warrior_id);

        TBattle battle(factory_.Create<TBattleMap>(map_id), &rng, io_system);
        battle.AddPlayer(
            TPlayer(&creature_a, TPlayerTeam{result.side_a.team}, TPlayerId{1}, result.side_a.name, "warrior.png"),
            TPosition{.x = 3, .y = 3});
        battle.AddPlayer(
            TPlayer(&creature_b, TPlayerTeam{result.side_b.team}, TPlayerId{2}, result.side_b.name, "warrior.png"),
            TPosition{.x = 4, .y = 3});

        battle.StartBattle();

        std::optional<int> winner = battle.Winner();
        if (winner == result.side_a.team) {
            ++result.side_a.wins;
        } else if (winner == result.side_b.team) {
            ++result.side_b.wins;
        } else {
            ++result.draws;
        }

        if (!creature_a.IsAlive()) {
            ++result.side_a.deaths;
        }
        if (!creature_b.IsAlive()) {
            ++result.side_b.deaths;
        }
    }

    result.iterations = iterations;
    return result;
}

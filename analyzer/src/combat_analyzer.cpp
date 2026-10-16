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
#include <nlohmann/json.hpp>

#include <filesystem>
#include <array>

namespace {

const std::filesystem::path kPathToData{kRootDirPath + "/pf2e_engine/data"};

}

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

        TBattle battle(factory_.Create<TBattleMap>(map_id), &rng, io_system, factory_.AbilityRules());
        battle.AddPlayer(
            TPlayer(&creature_a, TPlayerTeam{result.side_a.team}, TPlayerId{1}, result.side_a.name, "warrior.png"),
            TPosition{.x = 3, .y = 3});
        battle.AddPlayer(
            TPlayer(&creature_b, TPlayerTeam{result.side_b.team}, TPlayerId{2}, result.side_b.name, "warrior.png"),
            TPosition{.x = 4, .y = 3});

        strategy.BindBattle(battle);

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

TEncounterAnalysisResult TCombatAnalyzer::RunFighterAgainstWolves(
    size_t iterations, const THumanFighter1Options& fighter_options)
{
    TEncounterAnalysisResult result{
        .iterations = iterations,
        .combatants = {{.name = fighter_options.Name.value_or("Human fighter 1"), .team = 1},
                       {.name = "Wolf A", .team = 2}, {.name = "Wolf B", .team = 2}},
    };
    for (size_t iteration = 0; iteration < iterations; ++iteration) {
        TRandomGenerator rng(static_cast<int>(iteration));
        TAggressiveMeleeStrategy strategy;
        TAutomatedInteractionSystem io_system(strategy);
        std::array<TCreature, 3> creatures{
            CreateHumanFighter1(fighter_options), CreateWolf({.Name = "Wolf A"}),
            CreateWolf({.Name = "Wolf B"}),
        };
        nlohmann::json map = {{"x_size", 12}, {"y_size", 12}};
        TBattle battle(TBattleMap(map), &rng, io_system, factory_.AbilityRules());
        const std::array<TPosition, 3> positions{{{3, 3}, {6, 2}, {6, 4}}};
        for (size_t index = 0; index < creatures.size(); ++index) {
            battle.AddPlayer(TPlayer(&creatures[index], TPlayerTeam{result.combatants[index].team},
                TPlayerId{static_cast<int>(index)}, result.combatants[index].name, ""),
                positions[index], {.TieOrder = index == 0 ? 1 : 0});
        }
        strategy.BindBattle(battle);
        battle.StartBattle();
        const auto winner = battle.Winner();
        if (!winner) {
            ++result.draws;
        }
        for (size_t index = 0; index < creatures.size(); ++index) {
            if (winner == result.combatants[index].team) {
                ++result.combatants[index].wins;
            }
            if (creatures[index].IsDead()) {
                ++result.combatants[index].deaths;
            }
        }
    }
    return result;
}

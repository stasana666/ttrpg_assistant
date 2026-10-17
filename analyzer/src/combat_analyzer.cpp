#include <analyzer/combat_analyzer.h>

#include <analyzer/aggressive_melee_strategy.h>
#include <analyzer/automated_interaction_system.h>

#include <pf2e_engine/battle.h>
#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/random.h>


#include <deque>

TEncounterAnalysisResult TCombatAnalyzer::RunTwoFighters(size_t iterations)
{
    return Run(iterations, {
        {.Name = "Fighter A", .Team = 1, .Position = {3, 3}, .Create = [] { return CreateHumanFighter1({.Name = "Fighter A"}); }},
        {.Name = "Fighter B", .Team = 2, .Position = {4, 3}, .Create = [] { return CreateHumanFighter1({.Name = "Fighter B"}); }},
    });
}

TEncounterAnalysisResult TCombatAnalyzer::RunFighterAgainstWolves(
    size_t iterations, const THumanFighter1Options& fighter_options)
{
    return Run(iterations, {
        {.Name = fighter_options.Name.value_or("Human fighter 1"), .Team = 1, .Position = {3, 3},
         .Create = [fighter_options] { return CreateHumanFighter1(fighter_options); }},
        {.Name = "Wolf A", .Team = 2, .Position = {6, 2}, .Create = [] { return CreateWolf({.Name = "Wolf A"}); }},
        {.Name = "Wolf B", .Team = 2, .Position = {6, 4}, .Create = [] { return CreateWolf({.Name = "Wolf B"}); }},
    });
}

TEncounterAnalysisResult TCombatAnalyzer::Run(size_t iterations, const std::vector<TCombatant>& combatants)
{
    TEncounterAnalysisResult result;
    result.iterations = iterations;
    for (const auto& combatant : combatants) {
        result.combatants.push_back({.name = combatant.Name, .team = combatant.Team});
    }
    for (size_t iteration = 0; iteration < iterations; ++iteration) {
        TRandomGenerator rng(static_cast<int>(iteration));
        TAggressiveMeleeStrategy strategy;
        TAutomatedInteractionSystem io_system(strategy);
        std::deque<TCreature> creatures;
        TBattle battle(TBattleMap(12, 12), &rng, io_system, rules_);
        for (size_t index = 0; index < combatants.size(); ++index) {
            creatures.push_back(combatants[index].Create());
            battle.AddPlayer(TPlayer(&creatures.back(), TPlayerTeam{combatants[index].Team},
                TPlayerId{static_cast<int>(index)}, combatants[index].Name, ""),
                combatants[index].Position, {.TieOrder = index == 0 ? 1 : 0});
        }
        strategy.BindBattle(battle);
        battle.StartBattle();
        const auto winner = battle.Winner();
        if (!winner) {
            ++result.draws;
        }
        for (size_t index = 0; index < combatants.size(); ++index) {
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

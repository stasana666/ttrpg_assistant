#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/common/errors.h>

#include <pf2e_engine/interaction_system.h>
#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/random.h>

#include <cpp_config.h>

#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>

using FsPath = std::filesystem::path;
using FsDirEntry = std::filesystem::directory_entry;
using FsRecursiveIterator = std::filesystem::recursive_directory_iterator;

TInteractionSystem interaction_system;

const std::filesystem::path kPathToData{kRootDirPath + "/pf2e_engine/data"};

void InitGameObjects(TGameObjectFactory& factory)
{
    for (const FsDirEntry& dir_entry : FsRecursiveIterator(kPathToData)) {
        std::cout << dir_entry.path() << std::endl;
        if (dir_entry.is_directory()) {
            factory.AddSource(dir_entry.path());
        }
    }
}

TCreature CreateCreature(TGameObjectFactory& factory)
{
    std::function<std::string_view(const TGameObjectId&)> func = [](const TGameObjectId& id) { return TGameObjectIdManager::Instance().Name(id); };

    auto player_id = interaction_system.ChooseAlternative(0, factory.AllKnown<TCreature>(),
        "character", func);

    return factory.Create<TCreature>(player_id);
}

TBattleMap CreateBattleMap(TGameObjectFactory& factory)
{
    std::function<std::string_view(const TGameObjectId&)> func = [](const TGameObjectId& id) { return TGameObjectIdManager::Instance().Name(id); };

    auto battle_map_id = interaction_system.ChooseAlternative(0, factory.AllKnown<TBattleMap>(),
        "battle_map", func);

    return factory.Create<TBattleMap>(battle_map_id);
}

int main()
try
{
    TGameObjectFactory factory;
    InitGameObjects(factory);

    TCreature player_1 = CreateCreature(factory);
    TCreature player_2 = CreateCreature(factory);

    TRandomGenerator dice_roller(666);
    TBattle battle(CreateBattleMap(factory), &dice_roller);

    battle.AddPlayer(TPlayer{
        .team = 1,
        .position = {0, 0},
        .creature = &player_1,
        .name = "Jonn",
    });

    battle.AddPlayer(TPlayer{
        .team = 2,
        .position = {0, 1},
        .creature = &player_2,
        .name = "Artur",
    });

    battle.StartBattle();

    return 0;
}
catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    throw;
}
catch (...) {
    std::cerr << "Unknown error" << std::endl;
    throw;
}

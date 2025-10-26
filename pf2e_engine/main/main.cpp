#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/common/errors.h>

#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/random.h>

#include <cpp_config.h>

#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using FsPath = std::filesystem::path;
using FsDirEntry = std::filesystem::directory_entry;
using FsRecursiveIterator = std::filesystem::recursive_directory_iterator;

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
    std::cout << "Add player with id: ";
    std::string player_id;
    std::cin >> player_id;
    return factory.CreateCreature(TGameObjectIdManager::Instance().Register(player_id));
}

TBattleMap CreateBattleMap(TGameObjectFactory& factory)
{
    std::cout << "Choose battle map with id: ";
    std::string battle_map;
    std::cin >> battle_map;
    return factory.CreateBattleMap(TGameObjectIdManager::Instance().Register(battle_map));
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

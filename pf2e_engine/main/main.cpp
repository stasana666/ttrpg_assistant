#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/common/errors.h>

#include <pf2e_engine/gui/board.h>

#include <pf2e_engine/interaction_system.h>
#include <pf2e_engine/intent_recognizer.h>
#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/random.h>

#include <cpp_config.h>

#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <thread>
#include "pf2e_engine/common/channel.h"
#include "pf2e_engine/gui/click_event.h"
#include "pf2e_engine/player.h"

using FsPath = std::filesystem::path;
using FsDirEntry = std::filesystem::directory_entry;
using FsRecursiveIterator = std::filesystem::recursive_directory_iterator;

const std::filesystem::path kPathToData{kRootDirPath + "/pf2e_engine/data"};
const std::filesystem::path kPathToImages{kRootDirPath + "/pf2e_engine/images"};

void InitGameObjects(TGameObjectFactory& factory, TInteractionSystem& interaction_system)
{
    for (const FsDirEntry& dir_entry : FsRecursiveIterator(kPathToData)) {
        interaction_system.DevLog() << dir_entry.path() << std::endl;
        if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".json") {
            factory.AddSource(dir_entry.path());
        }
    }
}

TCreature CreateCreature(TGameObjectFactory& factory, TInteractionSystem& interaction_system)
{
    std::function<std::string(const TGameObjectId&)> func = [](const TGameObjectId& id)
        { return std::string(TGameObjectIdManager::Instance().Name(id)); };

    auto player_id = interaction_system.ChooseAlternative(0,
        TAlternatives<TGameObjectId>("character", factory.AllKnown<TCreature>(), func));

    return factory.Create<TCreature>(player_id);
}

TBattleMap CreateBattleMap(TGameObjectFactory& factory, TInteractionSystem& interaction_system)
{
    std::function<std::string(const TGameObjectId&)> func = [](const TGameObjectId& id)
        { return std::string(TGameObjectIdManager::Instance().Name(id)); };

    auto battle_map_id = interaction_system.ChooseAlternative(0,
        TAlternatives<TGameObjectId>("battle map", factory.AllKnown<TBattleMap>(), func));

    return factory.Create<TBattleMap>(battle_map_id);
}

int main(int argc, char** argv)
{
    TChannel<TClickEvent> chan(512);
    TInteractionSystem interaction_system(chan.MakeConsumer());

    std::unique_ptr<TUserIntentRecognizer> recognizer;
    if (argc > 1) {
        recognizer = std::make_unique<TUserIntentRecognizer>(argv[1]);
        return 0;
    }

    TGameObjectFactory factory;
    InitGameObjects(factory, interaction_system);

    TCreature player_1 = CreateCreature(factory, interaction_system);
    TCreature player_2 = CreateCreature(factory, interaction_system);

    TRandomGenerator dice_roller(666);
    TBattle battle(CreateBattleMap(factory, interaction_system), &dice_roller, interaction_system);
    TBoardGUI board(battle.BattleMapMutable(), kPathToImages, chan.MakeProducer());

    battle.AddPlayer(TPlayer(
            &player_1,
            TPlayerTeam{1},
            TPlayerId{1},
            "Jonn",
            "warrior.png"
        ),
        TPosition{
            .x = 1,
            .y = 1
        });

    battle.AddPlayer(TPlayer(
            &player_1,
            TPlayerTeam{2},
            TPlayerId{1},
            "Oown", // other obvios warrior name
            "warrior2.png"
        ),
        TPosition{
            .x = 0,
            .y = 0
        });

    std::thread game_logic_thread{[&]() {
        try {
         battle.StartBattle();
        } catch (const std::exception& err) {
            std::cerr << err.what() << std::endl;
            throw;
        }
        catch (...) {
            std::cerr << "Unknown error" << std::endl;
            throw;
        }
    }};

    board.Run();

    game_logic_thread.join();

    return 0;
}

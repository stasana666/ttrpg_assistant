#include <pf2e_engine/audio_input/audio_input.h>
#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/channel.h>
#include <pf2e_engine/common/errors.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/gui/board.h>
#include <pf2e_engine/interaction_system.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/random.h>
#include <pf2e_engine/audio_input/speech_to_text.h>
#include <pf2e_engine/audio_input/audio_input_system.h>
#include <pf2e_engine/common/config.h>

#include <cpp_config.h>

#include <argparse/argparse.hpp>

#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <thread>

using FsPath = std::filesystem::path;
using FsDirEntry = std::filesystem::directory_entry;
using FsRecursiveIterator = std::filesystem::recursive_directory_iterator;

const std::filesystem::path kPathToData{kRootDirPath + "/pf2e_engine/data"};
const std::filesystem::path kPathToImages{kRootDirPath + "/pf2e_engine/images"};

TConfig ParseArgs(int argc, char** argv)
{
    argparse::ArgumentParser program("pf2e_engine");

    program.add_argument("--speech2text")
        .help("Path to speech-to-text model");

    program.add_argument("--nlp-model")
        .help("Path to natural language analysis model");

    program.parse_args(argc, argv);

    TConfig cfg;
    cfg.speech_model = program.present("--speech2text");
    cfg.nlp_model = program.present("--nlp-model");

    return cfg;
}

void InitGameObjects(TGameObjectFactory& factory, [[maybe_unused]] TInteractionSystem& interaction_system)
{
    for (const FsDirEntry& dir_entry : FsRecursiveIterator(kPathToData)) {
        // interaction_system.DevLog() << dir_entry.path() << std::endl;
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
    TConfig cfg = ParseArgs(argc, argv);

    TChannel<TClickEvent> chan(512);
    TInteractionSystem interaction_system(chan.MakeConsumer());

    if (cfg.nlp_model.has_value() && cfg.speech_model.has_value()) {
        interaction_system.Add(std::make_unique<TAudioInputSystem>(cfg));
    } else {
        std::cerr << "Audio input subsystem is disabled because no speech or NLP models were provided." << std::endl;
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
            std::cerr << "Unknown error, unknown exception in main()" << std::endl;
            throw;
        }
    }};

    board.Run();

    game_logic_thread.join();

    return 0;
}

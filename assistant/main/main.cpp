#include <assistant/audio_input/audio_input.h>
#include <pf2e_engine/battle_map.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/channel.h>
#include <pf2e_engine/common/errors.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/rules.h>
#include <pf2e_engine/inventory/battle_maps.h>
#include <pf2e_engine/inventory/remaster_profiles.h>
#include <assistant/gui/board.h>
#include <assistant/interaction_system.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/random.h>
#include <assistant/audio_input/speech_to_text.h>
#include <assistant/audio_input/audio_input_system.h>
#include <pf2e_engine/common/config.h>

#include <cpp_config.h>

#include <argparse/argparse.hpp>

#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <thread>

const std::filesystem::path kPathToImages{kRootDirPath + "/assistant/images"};

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

TCreature CreateCreature(TInteractionSystem& interaction_system)
{
    using FProfile = std::function<TCreature()>;
    auto profiles = TAlternatives::Create<FProfile>("character");
    profiles.AddAlternative("Human fighter", FProfile{[] { return CreateHumanFighter1(); }});
    profiles.AddAlternative("Wolf", FProfile{[] { return CreateWolf(); }});
    profiles.AddAlternative("Elf wizard", FProfile{[] { return CreateElfWizard5(); }});
    return interaction_system.ChooseAlternative<FProfile>(0, profiles)();
}

TBattleMap CreateBattleMap(TInteractionSystem& interaction_system)
{
    using FLayout = std::function<TMapLayout()>;
    auto layouts = TAlternatives::Create<FLayout>("battle map");
    layouts.AddAlternative("Simple map", FLayout{[] { return CreateSimpleMap(); }});
    const auto layout = interaction_system.ChooseAlternative<FLayout>(0, layouts)();
    return TBattleMap(layout.Width(), layout.Height());
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

    TCreature player_1 = CreateCreature(interaction_system);
    TCreature player_2 = CreateCreature(interaction_system);

    TRandomGenerator dice_roller(666);
    TBattle battle(CreateBattleMap(interaction_system), &dice_roller, interaction_system,
                   CreateAbilityRules());
    TBoardGUI board(battle.BattleMapMutable(), kPathToImages, chan.MakeProducer());

    battle.AddPlayer(TPlayer(
            &player_1,
            TPlayerTeam{1},
            TPlayerId{1},
            "Jonn",
            "warrior.png"
        ),
        TPosition{
            .x = 3,
            .y = 3
        });

    battle.AddPlayer(TPlayer(
            &player_2,
            TPlayerTeam{2},
            TPlayerId{2},
            "Oown",
            "warrior2.png"
        ),
        TPosition{
            .x = 5,
            .y = 4
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

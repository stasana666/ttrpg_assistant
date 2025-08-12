#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <cpp_config.h>

#include <filesystem>

const std::filesystem::path kPathToCreature{kRootDirPath + "/pf2e_engine/data/creatures"};

int main()
{
    TGameObjectFactory factory;

    factory.AddSource(kPathToCreature);
    return 0;
}

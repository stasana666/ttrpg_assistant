#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <cpp_config.h>

#include <filesystem>
#include <iostream>

using FsPath = std::filesystem::path;
using FsDirEntry = std::filesystem::directory_entry;
using FsRecursiveIterator = std::filesystem::recursive_directory_iterator;

const std::filesystem::path kPathToData{kRootDirPath + "/pf2e_engine/data"};

void InitGameObjects(TGameObjectFactory& factory)
{

    for (const FsDirEntry& dir_entry : FsRecursiveIterator(kPathToData)) {
        std::cout << dir_entry.path() << std::endl;
        if (dir_entry.is_directory()) {
            //continue;
            factory.AddSource(dir_entry.path());
        }
    }
}

int main()
{
    TGameObjectFactory factory;
    InitGameObjects(factory);
    return 0;
}

#include <ttrpg/emit.h>
#include <ttrpg/module.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

struct TArgs {
    std::string SchemaPath;
    std::string OutH;
    std::string OutCpp;
};

TArgs ParseArgs(int argc, char** argv) {
    TArgs a;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto NextVal = [&]() -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing value for " + arg);
            }
            return argv[++i];
        };
        if (arg == "--schema") {
            a.SchemaPath = NextVal();
        } else if (arg == "--out-h") {
            a.OutH = NextVal();
        } else if (arg == "--out-cpp") {
            a.OutCpp = NextVal();
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }
    if (a.SchemaPath.empty() || a.OutH.empty() || a.OutCpp.empty()) {
        throw std::runtime_error(
            "usage: ttrpg_codegen --schema <path> --out-h <path> --out-cpp <path>");
    }
    return a;
}

std::string DerivePrimaryHeaderInclude(const std::string& outH) {
    const std::string marker = "/include/";
    auto pos = outH.rfind(marker);
    if (pos != std::string::npos) {
        return outH.substr(pos + marker.size());
    }
    return fs::path(outH).filename().string();
}

int main(int argc, char** argv) {
    try {
        TArgs args = ParseArgs(argc, argv);

        TLoadedSchemas loaded = LoadAll(args.SchemaPath);

        std::string sourceName = fs::path(args.SchemaPath).filename().string();
        std::string headerInclude = DerivePrimaryHeaderInclude(args.OutH);

        fs::create_directories(fs::path(args.OutH).parent_path());
        fs::create_directories(fs::path(args.OutCpp).parent_path());

        {
            std::ofstream out(args.OutH);
            if (!out) {
                throw std::runtime_error("cannot write: " + args.OutH);
            }
            EmitHeader(out, loaded, sourceName, args.OutH);
        }
        {
            std::ofstream out(args.OutCpp);
            if (!out) {
                throw std::runtime_error("cannot write: " + args.OutCpp);
            }
            EmitImpl(out, loaded, sourceName, headerInclude);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ttrpg_codegen: " << e.what() << "\n";
        return 1;
    }
}

#include <ttrpg/emit.h>
#include <ttrpg/module.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct TArgs {
    std::string SchemaPath;
    std::string OutH;
    std::string OutCpp;
    std::string Depfile;
    bool Check = false;
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
        } else if (arg == "--depfile") {
            a.Depfile = NextVal();
        } else if (arg == "--check") {
            a.Check = true;
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }
    if (a.SchemaPath.empty() || (!a.Check && (a.OutH.empty() || a.OutCpp.empty()))) {
        throw std::runtime_error(
            "usage: ttrpg_codegen --schema <path> "
            "(--check | --out-h <path> --out-cpp <path> [--depfile <path>])");
    }
    if (a.Check && (!a.OutH.empty() || !a.OutCpp.empty() || !a.Depfile.empty())) {
        throw std::runtime_error("--check cannot be combined with output paths or --depfile");
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

std::string EscapeDepfilePath(const fs::path& path) {
    std::string result;
    for (char c : path.generic_string()) {
        if (c == '\n' || c == '\r') {
            throw std::runtime_error("depfile paths cannot contain newlines");
        }
        if (c == '$') {
            result += "$$";
        } else {
            if (c == ' ' || c == '\t' || c == '#' || c == '\\' || c == ':') {
                result += '\\';
            }
            result += c;
        }
    }
    return result;
}

std::string MakeDepfile(const TArgs& args, const TLoadedSchemas& loaded) {
    std::ostringstream out;
    out << EscapeDepfilePath(fs::absolute(args.OutH).lexically_normal()) << " "
        << EscapeDepfilePath(fs::absolute(args.OutCpp).lexically_normal()) << ":";
    auto dependencies = loaded.SourcePaths;
    std::sort(dependencies.begin(), dependencies.end());
    for (const auto& path : dependencies) {
        out << " " << EscapeDepfilePath(path);
    }
    out << "\n";
    return out.str();
}

void ValidateOutputPaths(const TArgs& args, const TLoadedSchemas& loaded) {
    std::vector<fs::path> outputs;
    for (const auto& output : {args.OutH, args.OutCpp, args.Depfile}) {
        if (output.empty()) continue;
        const auto path = fs::weakly_canonical(fs::absolute(output));
        const auto same = [&](const fs::path& other) {
            std::error_code error;
            return path == other || fs::equivalent(path, other, error);
        };
        if (std::any_of(outputs.begin(), outputs.end(), same)) throw std::runtime_error("output paths must be distinct: " + output);
        if (std::any_of(loaded.SourcePaths.begin(), loaded.SourcePaths.end(), same)) throw std::runtime_error("output would overwrite a schema: " + output);
        outputs.push_back(path);
    }
}

void WriteFile(const fs::path& path, const std::string& contents) {
    if (path.has_parent_path()) fs::create_directories(path.parent_path());
    const fs::path temporary = path.string() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary);
        out.exceptions(std::ios::failbit | std::ios::badbit);
        out << contents;
    }
    fs::rename(temporary, path);
}

int main(int argc, char** argv) {
    try {
        TArgs args = ParseArgs(argc, argv);

        TLoadedSchemas loaded = LoadAll(args.SchemaPath);

        if (args.Check) {
            for (const auto& path : loaded.SourcePaths) {
                loaded.PrimaryStem = path.stem().string();
                loaded.Primary = loaded.ByStem.at(loaded.PrimaryStem);
                std::ostringstream header;
                std::ostringstream implementation;
                std::string virtualHeader = loaded.PrimaryStem + ".h";
                EmitHeader(header, loaded, path.filename().string(), virtualHeader);
                EmitImpl(implementation, loaded, path.filename().string(), virtualHeader);
            }
            return 0;
        }
        ValidateOutputPaths(args, loaded);
        std::string sourceName = fs::path(args.SchemaPath).filename().string();
        std::string headerInclude = DerivePrimaryHeaderInclude(args.OutH);

        std::ostringstream header;
        std::ostringstream implementation;
        EmitHeader(header, loaded, sourceName, args.OutH);
        EmitImpl(implementation, loaded, sourceName, headerInclude);
        std::string depfile;
        if (!args.Depfile.empty()) {
            depfile = MakeDepfile(args, loaded);
        }

        WriteFile(args.OutH, header.str());
        WriteFile(args.OutCpp, implementation.str());
        if (!args.Depfile.empty()) {
            WriteFile(args.Depfile, depfile);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ttrpg_codegen: " << e.what() << "\n";
        return 1;
    }
}

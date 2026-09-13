#include <ttrpg/emit.h>
#include <ttrpg/module.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>
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

class TOutputTransaction {
public:
    ~TOutputTransaction() {
        for (const auto& file : Files_) {
            if (!file.HasBackup) {
                std::error_code error;
                fs::remove_all(file.StagingDirectory, error);
            }
        }
    }

    void Stage(const fs::path& path, const std::string& contents) {
        fs::path absolute = fs::absolute(path).lexically_normal();
        fs::create_directories(absolute.parent_path());
        std::error_code error;
        auto status = fs::symlink_status(absolute, error);
        if (error && error != std::errc::no_such_file_or_directory) {
            throw fs::filesystem_error("cannot inspect output", absolute, error);
        }
        if (fs::exists(status) && !fs::is_regular_file(status)) {
            throw std::runtime_error("output is not a regular file: " + absolute.string());
        }

        fs::path directory;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (size_t attempt = 0;; ++attempt) {
            directory = absolute.parent_path() /
                (absolute.filename().string() + ".ttrpg-" + std::to_string(stamp) +
                 "-" + std::to_string(attempt));
            if (fs::create_directory(directory)) {
                break;
            }
        }
        Files_.push_back({absolute, directory, false, false});
        std::ofstream out(directory / "new", std::ios::binary);
        out.exceptions(std::ios::failbit | std::ios::badbit);
        out << contents;
        out.close();
    }

    void Publish() {
        try {
            for (auto& file : Files_) {
                if (fs::exists(file.Path)) {
                    fs::rename(file.Path, file.StagingDirectory / "old");
                    file.HasBackup = true;
                }
                fs::rename(file.StagingDirectory / "new", file.Path);
                file.Published = true;
            }
        } catch (const std::exception& error) {
            std::string failure = error.what();
            for (auto it = Files_.rbegin(); it != Files_.rend(); ++it) {
                std::error_code restoreError;
                if (it->Published) {
                    fs::remove(it->Path, restoreError);
                }
                if (!restoreError && it->HasBackup) {
                    fs::rename(it->StagingDirectory / "old", it->Path, restoreError);
                    if (!restoreError) {
                        it->HasBackup = false;
                    }
                }
                if (restoreError) {
                    failure += "; cannot restore '" + it->Path.string() + "': " +
                        restoreError.message() + "; staged files retained in '" +
                        it->StagingDirectory.string() + "'";
                }
            }
            throw std::runtime_error(failure);
        }
        for (auto& file : Files_) {
            file.HasBackup = false;
        }
    }

private:
    struct TFile {
        fs::path Path;
        fs::path StagingDirectory;
        bool HasBackup;
        bool Published;
    };

    std::vector<TFile> Files_;
};

void ValidateOutputPaths(const TArgs& args, const TLoadedSchemas& loaded) {
    const auto equivalent = [](const fs::path& first, const fs::path& second) {
        if (first == second) {
            return true;
        }
        std::error_code error;
        const bool same = fs::equivalent(first, second, error);
        if (error && error != std::errc::no_such_file_or_directory
            && error != std::errc::not_a_directory) {
            throw fs::filesystem_error("cannot compare output paths", first, second, error);
        }
        return same;
    };
    std::vector<fs::path> outputs;
    for (const auto& path : {args.OutH, args.OutCpp, args.Depfile}) {
        if (path.empty()) {
            continue;
        }
        fs::path canonical = fs::weakly_canonical(fs::absolute(path));
        for (const auto& previous : outputs) {
            if (equivalent(canonical, previous)) {
                throw std::runtime_error("output paths must be distinct: " + path);
            }
            auto [first, second] = std::mismatch(
                canonical.begin(), canonical.end(), previous.begin(), previous.end());
            if (first == canonical.end() || second == previous.end()) {
                throw std::runtime_error("output paths cannot contain another output: " + path);
            }
        }
        if (std::any_of(loaded.SourcePaths.begin(), loaded.SourcePaths.end(),
                       [&](const auto& source) { return equivalent(canonical, source); })) {
            throw std::runtime_error("output would overwrite a schema: " + path);
        }
        outputs.push_back(std::move(canonical));
    }
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

        TOutputTransaction outputs;
        outputs.Stage(args.OutH, header.str());
        outputs.Stage(args.OutCpp, implementation.str());
        if (!args.Depfile.empty()) {
            outputs.Stage(args.Depfile, depfile);
        }
        outputs.Publish();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ttrpg_codegen: " << e.what() << "\n";
        return 1;
    }
}

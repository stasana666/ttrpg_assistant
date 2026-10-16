#pragma once

#include <ttrpg/emit.h>
#include <ttrpg/module.h>
#include <ttrpg/parser.h>

#include <gtest/gtest.h>

#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

inline TLoadedSchemas LoadRules(const std::string& stem, const std::string& source) {
    TLoadedSchemas loaded;
    loaded.PrimaryStem = stem;
    loaded.Primary = TParser(Tokenize(source)).Parse();
    RegisterModuleSymbols(stem, loaded.Primary, loaded.SymbolTable);
    return loaded;
}

inline std::string EmitHeaderText(const TLoadedSchemas& loaded, const std::string& name) {
    std::ostringstream output;
    EmitHeader(output, loaded, name + ".ttrpg", name + ".h");
    return output.str();
}

inline std::string EmitImplText(const TLoadedSchemas& loaded, const std::string& name) {
    std::ostringstream output;
    EmitImpl(output, loaded, name + ".ttrpg", name + ".h");
    return output.str();
}

inline std::string Join(const std::vector<std::string>& lines) {
    std::string result;
    for (const auto& line : lines) result += line + "\n";
    return result;
}

inline void ExpectRejected(const std::string& source, const std::string& diagnostic, const std::function<void()>& compile) {
    try {
        compile();
        ADD_FAILURE() << "expected rejection: " << source;
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find(diagnostic), std::string::npos) << error.what();
    }
}

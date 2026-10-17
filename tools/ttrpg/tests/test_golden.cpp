#include <gtest/gtest.h>

#include <ttrpg/emit.h>
#include <ttrpg/module.h>
#include <ttrpg/parser.h>

#include <fstream>
#include <sstream>
#include <string>

#ifndef TTRPG_FIXTURES_DIR
#error "TTRPG_FIXTURES_DIR must be defined by the build"
#endif

namespace {

const std::string kFixtures = TTRPG_FIXTURES_DIR;


std::string ReadFile(const std::string& path) {
    std::ifstream in(path);
    EXPECT_TRUE(in.good()) << "cannot open " << path;
    std::stringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

std::string GenHeader(const std::string& name) {
    TLoadedSchemas loaded = LoadAll(kFixtures + "/" + name + ".ttrpg");
    std::ostringstream os;
    EmitHeader(os, loaded, name + ".ttrpg", "include/" + name + ".h");
    return os.str();
}

std::string GenImpl(const std::string& name) {
    TLoadedSchemas loaded = LoadAll(kFixtures + "/" + name + ".ttrpg");
    std::ostringstream os;
    EmitImpl(os, loaded, name + ".ttrpg", name + ".h");
    return os.str();
}

void CheckGolden(const std::string& name) {
    EXPECT_EQ(GenHeader(name), ReadFile(kFixtures + "/" + name + ".golden.h"))
        << name << " header drifted from golden";
    EXPECT_EQ(GenImpl(name), ReadFile(kFixtures + "/" + name + ".golden.cpp"))
        << name << " impl drifted from golden";
}

}

TEST(GoldenTest, Basic) {
    CheckGolden("basic");
}

TEST(GoldenTest, Variant) {
    CheckGolden("variant");
}

TEST(GoldenTest, Computed) {
    CheckGolden("computed");
}

TEST(GoldenTest, Collection) {
    CheckGolden("collection");
}

TEST(GoldenTest, EmptyStorageFactoriesCreateThroughTheResolver) {
    TLoadedSchemas loaded;
    loaded.PrimaryStem = "empty_storage";
    loaded.Primary = TParser(Tokenize(R"(
        class TQueries { query int One() { return 1; } }
        class TDerived { derive int One = max(1, 1); }
        GameEntity TMarker {}
    )")).Parse();
    RegisterModuleSymbols(loaded.PrimaryStem, loaded.Primary, loaded.SymbolTable);
    std::ostringstream output;
    EmitImpl(output, loaded, "empty_storage.ttrpg", "empty_storage.h");
    const auto code = output.str();
    for (const std::string name : {"TQueries", "TDerived", "TMarker"}) {
        const auto create = code.find(name + " " + name + "::Create(");
        ASSERT_NE(create, std::string::npos);
        EXPECT_NE(code.find("ResolveInitialValues(initial)", create), std::string::npos);
    }
}

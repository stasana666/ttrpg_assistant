#include <gtest/gtest.h>

#include <ttrpg/emit.h>
#include <ttrpg/module.h>

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

TEST(GoldenTest, MapCodegen) {
    const std::string header = GenHeader("map");
    const std::string impl = GenImpl("map");

    EXPECT_NE(header.find("#include <map>"), std::string::npos);
    EXPECT_NE(header.find("const std::map<EColor, int>& Counts() const"), std::string::npos);
    EXPECT_NE(header.find("TGuarded<std::map<EColor, int>> Counts()"), std::string::npos);
    EXPECT_NE(impl.find("r.Counts_.emplace(EColorFromString(map_key), map_value.get<int>())"),
              std::string::npos);
    EXPECT_NE(impl.find("AddValueField(map_node, ToString(map_key), map_value)"),
              std::string::npos);
}

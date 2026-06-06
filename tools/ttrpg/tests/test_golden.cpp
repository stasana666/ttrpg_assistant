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

// Mirror the CLI: header sees an out-h under .../include/<name>.h (only matters
// for cross-file sibling includes, which these self-contained fixtures lack);
// the impl includes "<name>.h".
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

}  // namespace

// `basic`: enum + class with primitive/default/enum/set<enum> fields.
TEST(GoldenTest, Basic) {
    CheckGolden("basic");
}

// `variant`: enum + variant (flag / single-field int / single-field enum /
// multi-field with default) + class with set<variant> -> TVariantMap.
TEST(GoldenTest, Variant) {
    CheckGolden("variant");
}

// `computed`: a class with computed (field-referencing) defaults -- an int and a
// BoundedQuantity, with a forward reference and a constant default mixed in.
TEST(GoldenTest, Computed) {
    CheckGolden("computed");
}

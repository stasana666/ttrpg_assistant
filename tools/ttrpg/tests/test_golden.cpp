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

TEST(GoldenTest, ResourceCodegen) {
    const std::string header = GenHeader("resource");
    const std::string impl = GenImpl("resource");

    EXPECT_NE(header.find("#include <pf2e_engine/common/resource.h>"), std::string::npos);
    EXPECT_NE(header.find("const TResource& Actions() const"), std::string::npos);
    EXPECT_NE(header.find("TGuarded<TResource> Actions()"), std::string::npos);
    EXPECT_NE(header.find("const std::map<ESlot, TResource>& Slots() const"), std::string::npos);
    EXPECT_NE(header.find("TResource Actions_{0};"), std::string::npos);

    EXPECT_NE(impl.find("r.Actions_ = j.contains(\"actions\") ? "
                        "TResource::FromJson(j.at(\"actions\"), factory) : TResource(0)"),
              std::string::npos);
    EXPECT_NE(impl.find("r.Slots_.emplace(ESlotFromString(map_key), "
                        "TResource::FromJson(map_value, factory))"),
              std::string::npos);
    EXPECT_NE(impl.find("AddOwnedObject(node, \"actions\", Actions_, ctx)"), std::string::npos);
    EXPECT_NE(impl.find("AddOwnedObject(map_node, ToString(map_key), map_value, ctx)"),
              std::string::npos);
}

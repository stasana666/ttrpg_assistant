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

TEST(GoldenTest, MapCodegen) {
    const std::string header = GenHeader("map");
    const std::string impl = GenImpl("map");

    EXPECT_NE(header.find("#include <map>"), std::string::npos);
    EXPECT_NE(header.find("const std::map<EColor, int>& Counts() const"), std::string::npos);
    EXPECT_NE(header.find("TGuarded<std::map<EColor, int>> Counts()"), std::string::npos);
    EXPECT_NE(impl.find("r.Counts_.emplace(EColorFromString(map_key), ::ReadGeneratedRuleInteger(map_value))"),
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

TEST(GoldenTest, SharesCheckedJsonIntegerParsingAcrossStorageAndDefaults) {
    TLoadedSchemas loaded;
    loaded.PrimaryStem = "checked_json";
    loaded.Primary = TParser(Tokenize(R"(
        enum EKey { First }
        variant TValue { Number { int Amount; } }
        class TLiteral {
            int Required;
            int Defaulted = 3;
            bool Enabled = true;
            string Name;
            map<EKey, int> Values;
        }
        class TRule {
            int Value = 0;
            derive int Result = @self.Value;
        }
        class TShadow { int ReadGeneratedRuleInteger = 0; }
    )")).Parse();
    RegisterModuleSymbols(loaded.PrimaryStem, loaded.Primary, loaded.SymbolTable);
    std::ostringstream output;
    EmitImpl(output, loaded, "checked_json.ttrpg", "checked_json.h");
    const auto code = output.str();
    const auto helper = code.find("int ReadGeneratedRuleInteger(");
    ASSERT_NE(helper, std::string::npos);
    EXPECT_EQ(code.find("int ReadGeneratedRuleInteger(", helper + 1), std::string::npos);
    EXPECT_NE(code.find("if (!value.is_number_integer())"), std::string::npos);
    EXPECT_NE(code.find("value.is_number_unsigned() && value.get<std::uint64_t>()"), std::string::npos);
    EXPECT_NE(code.find("TRuleInteger::Checked(value.get<std::int64_t>())"), std::string::npos);
    EXPECT_NE(code.find("r.Defaulted_ = j.contains(\"defaulted\") ? ::ReadGeneratedRuleInteger(j.at(\"defaulted\")) : int{3}"), std::string::npos);
    EXPECT_NE(code.find("initial.Value = ::ReadGeneratedRuleInteger(j.at(\"value\"))"), std::string::npos);
    EXPECT_NE(code.find("a.Amount = ::ReadGeneratedRuleInteger(val)"), std::string::npos);
    EXPECT_NE(code.find("r.ReadGeneratedRuleInteger_ = j.contains(\"read_generated_rule_integer\") ? ::ReadGeneratedRuleInteger("), std::string::npos);
    EXPECT_NE(code.find("ReadGeneratedRuleInteger(map_value)"), std::string::npos);
    EXPECT_EQ(code.find(".get<int>()"), std::string::npos);
    EXPECT_EQ(code.find("j.value(\"defaulted\""), std::string::npos);

    TClassDecl conflicting;
    conflicting.Name = "ReadGeneratedRuleInteger";
    loaded.Primary.Classes.push_back(std::move(conflicting));
    loaded.SymbolTable.emplace("ReadGeneratedRuleInteger", TTypeInfo{"checked_json", ETypeKind::Class});
    EXPECT_THROW(EmitImpl(output, loaded, "checked_json.ttrpg", "checked_json.h"), std::runtime_error);
}

TEST(GoldenTest, LegacyEnumDefaultsDoNotMakeRequiredOrUnknownValuesOptional) {
    TLoadedSchemas loaded;
    loaded.PrimaryStem = "enum_defaults";
    loaded.Primary = TParser(Tokenize(R"(
        enum EMode { Normal, Alternate }
        class TDefaults { EMode Optional = Alternate; EMode Required; }
    )")).Parse();
    RegisterModuleSymbols(loaded.PrimaryStem, loaded.Primary, loaded.SymbolTable);
    std::ostringstream output;
    EmitImpl(output, loaded, "enum_defaults.ttrpg", "enum_defaults.h");
    const auto code = output.str();
    EXPECT_NE(code.find("r.Optional_ = j.contains(\"optional\") ? EModeFromString(j.at(\"optional\").get<std::string>()) : EMode::Alternate;"), std::string::npos);
    EXPECT_NE(code.find("r.Required_ = EModeFromString(j.at(\"required\").get<std::string>());"), std::string::npos);
    EXPECT_NE(code.find("throw std::runtime_error(\"unknown EMode:"), std::string::npos);
}

TEST(GoldenTest, EmptyStorageFactoriesDoNotDeclareUnusedRestorationValues) {
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
        const auto fromJson = code.find(name + " " + name + "::FromJson(");
        const auto restore = code.find(name + " " + name + "::Restore(");
        const auto toJson = code.find("nlohmann::json " + name + "::ToJson(");
        ASSERT_NE(create, std::string::npos);
        ASSERT_NE(fromJson, std::string::npos);
        ASSERT_NE(restore, std::string::npos);
        ASSERT_NE(toJson, std::string::npos);
        EXPECT_NE(code.substr(create, fromJson - create).find("ResolveInitialValues(initial)"), std::string::npos);
        EXPECT_NE(code.substr(fromJson, restore - fromJson).find("return Create(initial);"), std::string::npos);
        EXPECT_EQ(code.substr(restore, toJson - restore).find("TInitialValues initial;"), std::string::npos);
    }
}

TEST(GoldenTest, CompoundEntityLifecycleKeepsSnapshotsSeparateFromContextIdentity) {
    TLoadedSchemas loaded;
    loaded.PrimaryStem = "compound_entity";
    loaded.Primary = TParser(Tokenize(R"(
        enum ESlot { First, Second, }
        enum EResistance { Heat, Cold, }
        variant TCondition { Clear; Penalty { int Amount; } }
        class TPoints {
            int Maximum = 10;
            int Current = @self.Maximum;
            method void Reduce(int Amount) { @self.Current -= Amount; }
        }
        action Strike;
        feature Training { grant Strike; }
        class TItem { string Name; grant Strike; }
        GameEntity TActor {
            string Name;
            int Level = 1;
            TPoints Hitpoints = TPoints { Maximum = @self.Level * 10; };
            Resource Actions;
            Resource Reactions;
            Resource Hands;
            map<ESlot, Resource> SpellSlots;
            collection<TItem> Weapons;
            collection<Feature> Features;
            set<TCondition> Conditions;
            map<EResistance, int> Resistances;
            grant @self.Weapons;
            grant @self.Features;
        }
        TActor Example {
            Name = "Example";
            Weapons = [TItem { Name = "Sword"; }];
            Features = [Training];
        }
    )")).Parse();
    loaded.ByStem.emplace(loaded.PrimaryStem, loaded.Primary);
    RegisterModuleSymbols(loaded.PrimaryStem, loaded.Primary, loaded.SymbolTable);
    std::ostringstream header;
    std::ostringstream implementation;
    EmitHeader(header, loaded, "compound_entity.ttrpg", "compound_entity.h");
    EmitImpl(implementation, loaded, "compound_entity.ttrpg", "compound_entity.h");
    const std::string declaration = header.str();
    const std::string source = implementation.str();
    EXPECT_NE(declaration.find("TActor(const TActor&) = delete;"), std::string::npos);
    EXPECT_NE(declaration.find("std::optional<TIdCollection<TItem>> Weapons{};"), std::string::npos);
    EXPECT_NE(source.find("Id_(std::exchange(other.Id_, {}))"), std::string::npos);
    EXPECT_NE(source.find("Weapons_(std::move(other.Weapons_))"), std::string::npos);
    EXPECT_NE(source.find("SpellSlots_ = std::move(other.SpellSlots_);"), std::string::npos);
    EXPECT_NE(source.find("result.Id_ = NewEntityId();"), std::string::npos);
    EXPECT_NE(source.find("result.Hitpoints_ = initial.Hitpoints ? *initial.Hitpoints : TPoints::Create("), std::string::npos);

    const auto ast_start = source.find("TAstNode TActor::GetAst(");
    const auto ast_end = source.find("void TActor::RegisterDslProperties", ast_start);
    ASSERT_NE(ast_start, std::string::npos);
    const std::string ast = source.substr(ast_start, ast_end - ast_start);
    const auto fallback = ast.find("if (ctx.IdentityOf(this).empty())");
    const auto identity = ast.find("AddValueField(node, \"id\", ctx.IdentityOf(this));");
    EXPECT_LT(fallback, identity);
    EXPECT_EQ(ast.find("AddValueField(node, \"id\", Id_.Value)"), std::string::npos);
    EXPECT_NE(ast.find("ctx.RegisterIdentity(this, \"entity#\" + std::to_string(Id_.Value));"), std::string::npos);
    for (const std::string field : {"Hitpoints", "Actions", "Reactions", "Hands"}) {
        EXPECT_NE(ast.find("ctx.RegisterIdentity(&" + field + "_, owner_identity +"), std::string::npos) << field;
    }
    EXPECT_NE(ast.find("owner_identity + \".spell_slots.\" + ToString(key)"), std::string::npos);
    EXPECT_NE(ast.find("owner_identity + \".weapons.\" + std::to_string(entry.Id().Value)"), std::string::npos);
    EXPECT_NE(ast.find("AddValueField(container, \"next_id\", Weapons_.NextId().Value)"), std::string::npos);

    const auto restore_start = source.find("TActor TActor::Restore(");
    const auto restore_end = source.find("nlohmann::json TActor::ToJson()", restore_start);
    ASSERT_NE(restore_start, std::string::npos);
    const std::string restore = source.substr(restore_start, restore_end - restore_start);
    EXPECT_NE(restore.find("TActor result(TUninitialized{}, std::move(*initial.Name)"), std::string::npos);
    EXPECT_NE(restore.find("TPoints::Restore(j.at(\"hitpoints\"), factory)"), std::string::npos);
    EXPECT_NE(restore.find("result.Id_ = RestoreEntityId(j.at(\"id\").get<std::uint64_t>());"), std::string::npos);
    EXPECT_EQ(restore.find("return Create(initial)"), std::string::npos);
    EXPECT_EQ(restore.find("NewEntityId()"), std::string::npos);
    EXPECT_NE(source.find("result[\"id\"] = Id_.Value;"), std::string::npos);

    const auto load_start = source.find("TActor TActor::FromJson(");
    const std::string load = source.substr(load_start, restore_start - load_start);
    EXPECT_NE(load.find("return Create(initial);"), std::string::npos);
    EXPECT_EQ(load.find("j.at(\"id\")"), std::string::npos);
}

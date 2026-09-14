#include <gtest/gtest.h>

#include <ttrpg/emit.h>
#include <ttrpg/parser.h>
#include <ttrpg/program.h>

#include <sstream>
#include <stdexcept>
#include <string>

namespace {

const std::string kTypes = R"(
    enum ECategory { Simple, Martial }
    enum EDie { D6, D10 }
    variant TTrait {
        Agile;
        Fatal { EDie Die; }
        Thrown { int Range; }
    }
    action External;
    feature Training { grant External; }
    class TItem {
        string Name;
        int Size = 6;
        ECategory Category;
        set<TTrait> Traits;
        Feature Selection = Training;
        grant @self.Selection;
        method void Grow(int Amount) { @self.Size += Amount; }
    }
    GameEntity TActor {
        int Level = 1;
        Resource Effort = 3;
        collection<TItem> Inventory;
        grant @self.Inventory;
    }
)";

TLoadedSchemas Rules(const std::string& source) {
    TLoadedSchemas loaded;
    loaded.PrimaryStem = "sources";
    loaded.Primary = TParser(Tokenize(kTypes + source)).Parse();
    RegisterModuleSymbols(loaded.PrimaryStem, loaded.Primary, loaded.SymbolTable);
    return loaded;
}

std::string Header(const TLoadedSchemas& loaded) {
    std::ostringstream result;
    EmitHeader(result, loaded, "sources.ttrpg", "sources.h");
    return result.str();
}

std::string Implementation(const TLoadedSchemas& loaded) {
    std::ostringstream result;
    EmitImpl(result, loaded, "sources.ttrpg", "sources.h");
    return result.str();
}

std::string Join(const std::vector<std::string>& lines) {
    std::string result;
    for (const auto& line : lines) result += line + "\n";
    return result;
}

void Rejects(const std::string& source, const std::string& diagnostic) {
    try {
        Implementation(Rules(source));
        FAIL() << "expected rejection: " << source;
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find(diagnostic), std::string::npos) << error.what();
    }
}

}

TEST(SourceCompilerTest, BindsTypedSnapshotsAndValidatesUnusedSourcesAfterChoice) {
    const auto loaded = Rules(R"(
        action Probe for TActor using TItem Item {
            cost @self.Effort = 1;
            let Size = Item.Size;
            choose TActor Target;
            Target.Level += Size;
        }
    )");
    const auto& action = loaded.Primary.Abilities.back();
    EXPECT_EQ(action.SourceType, "TItem");
    EXPECT_EQ(action.SourceName, "Item");
    const auto body = Join(CompileAction(action, loaded).Body);
    EXPECT_NE(body.find("auto source_Item = ActionSource<TActor, TItem>(Context)"), std::string::npos);
    EXPECT_NE(body.find("captured_Item = source_Item"), std::string::npos);
    EXPECT_NE(body.find("auto source_Item = captured_Item"), std::string::npos);
    const auto callback = body.rfind("auto source_Item = captured_Item");
    ASSERT_NE(callback, std::string::npos);
    EXPECT_NE(body.find("source_Item.Validate();", callback), std::string::npos);
    EXPECT_EQ(body.find("source_Item.Guard()"), std::string::npos);
    EXPECT_EQ(body.find("source_Item.Read().Id()"), std::string::npos);
    const auto implementation = Implementation(loaded);
    EXPECT_NE(implementation.find("RequiredSource<TActor, TItem>(actor, source)"), std::string::npos);
    EXPECT_NE(implementation.find("PrepareActionSource<TActor, TItem>(Context, actor, grant)"), std::string::npos);
    EXPECT_NE(implementation.find("static const auto execution_Probe = std::make_shared<const TAbilityExecution>"), std::string::npos);
    EXPECT_NE(implementation.find("rules.Executions.Register(\"sources.Probe\", execution_Probe)"), std::string::npos);
}

TEST(SourceCompilerTest, VisitsSchemaPathsThroughOrdinaryAncestorsAndCollections) {
    const auto loaded = Rules(R"(
        class TBox { TItem Inner; }
        class TPlain { int Value; }
        GameEntity THolder {
            TBox Box;
            collection<TBox> Boxes;
            grant @self.Box.Inner.Selection;
        }
    )");
    const auto header = Header(loaded);
    EXPECT_NE(header.find("void VisitAbilitySources(TVisitor& visitor, const TAbilitySourcePath& path = {}) const"), std::string::npos);
    EXPECT_NE(header.find("visitor(*this, path);"), std::string::npos);
    EXPECT_NE(header.find("Box_.VisitAbilitySources(visitor, path.Field(\"box\"))"), std::string::npos);
    EXPECT_NE(header.find("Inner_.VisitAbilitySources(visitor, path.Field(\"inner\"))"), std::string::npos);
    EXPECT_NE(header.find("entry->VisitAbilitySources(visitor, path.Field(\"boxes\").Entry(entry.Id().Value))"), std::string::npos);
    EXPECT_EQ(header.find("Selection_.VisitAbilitySources"), std::string::npos);
}

TEST(SourceCompilerTest, ReadsTypedSetAndVariantPayloadValues) {
    const auto loaded = Rules(R"(
        class TProfile {
            set<ECategory> Favored = [Simple];
            query int Bonus(TItem Item) {
                let Amount = 0;
                if (@self.Favored.has(Item.Category) && Item.Traits.has(Agile)) { Amount += 1; }
                let ThrownData = Item.Traits.get(Thrown, { Range = 0; });
                if (Item.Traits.get(Fatal, { Die = D6; }).Die == D10) { Amount += 2; }
                return Amount + ThrownData.Range;
            }
        }
    )");
    const auto implementation = Implementation(loaded);
    EXPECT_NE(implementation.find("values.insert(ECategory::Simple)"), std::string::npos);
    EXPECT_NE(implementation.find(".contains((static_cast<const TItem&>(arg_Item)).Category())"), std::string::npos);
    EXPECT_NE(implementation.find(".Has(ETraitKind::Agile)"), std::string::npos);
    EXPECT_NE(implementation.find("rule_set.template Get<TTraitThrown>()"), std::string::npos);
    EXPECT_NE(implementation.find("TTraitThrown{.Range = 0}"), std::string::npos);
    EXPECT_NE(implementation.find("TTraitFatal{.Die = EDie::D6}"), std::string::npos);
    EXPECT_NE(implementation.find("(local_ThrownData).Range"), std::string::npos);
    EXPECT_EQ(implementation.find("local_ThrownData).Range()"), std::string::npos);
}

TEST(SourceCompilerTest, CreatesTypedMapSetAndVariantLiterals) {
    const auto loaded = Rules(R"(
        class TProfile { map<ECategory, TItem> Items; }
        TItem Blade {
            Name = "Blade";
            Traits = [Agile, Fatal { Die = D10; }];
        }
        TProfile Profile {
            Items = {
                Simple = Blade;
                Martial = { Size = 12; Traits = [Thrown { Range = 5; }]; };
            };
        }
    )");
    const auto implementation = Implementation(loaded);
    EXPECT_NE(implementation.find("TTrait(TTraitFatal{.Die = EDie::D10})"), std::string::npos);
    EXPECT_NE(implementation.find("values.emplace(ECategory::Simple, CreateBlade())"), std::string::npos);
    EXPECT_NE(implementation.find("values.emplace(ECategory::Martial, TItem::Create({.Size = 12"), std::string::npos);
    EXPECT_NE(implementation.find("TTrait(TTraitThrown{.Range = 5})"), std::string::npos);
}

TEST(SourceCompilerTest, ChecksDynamicVariantDuplicatesWithoutRepeatingEvaluation) {
    const auto loaded = Rules(R"(
        class TProfile {
            TTrait First = Agile;
            TTrait Second = Fatal { Die = D10; };
            set<TTrait> Traits = [@self.First, @self.Second];
        }
    )");
    const auto compiled = CompileClass(loaded.Primary.Classes.back(), loaded);
    const auto& initializer = compiled.Fields.back().Initializer;
    EXPECT_NE(initializer.find("auto element_0 = (std::as_const(self)).First();"), std::string::npos);
    EXPECT_NE(initializer.find("auto element_1 = (std::as_const(self)).Second();"), std::string::npos);
    EXPECT_NE(initializer.find("if (values.Has(element_1.Kind()))"), std::string::npos);
    EXPECT_NE(initializer.find("values.Set(std::move(element_1))"), std::string::npos);
    const auto first = initializer.find(".First()");
    EXPECT_EQ(initializer.find(".First()", first + 1), std::string::npos);
}

TEST(SourceCompilerTest, CopiesNamedValuesAndRejectsEntityValueCopies) {
    auto loaded = Rules(R"(
        TItem Blade { Size = 4; }
        class TLoadout {
            TItem Main = Blade;
            collection<TItem> Items;
        }
        TLoadout Pair { Items = [Blade, Blade]; }
    )");
    const auto implementation = Implementation(loaded);
    EXPECT_NE(implementation.find("result.Main_ = initial.Main ? *initial.Main : CreateBlade();"), std::string::npos);
    EXPECT_NE(implementation.find("values.Add(CreateBlade()); values.Add(CreateBlade());"), std::string::npos);
    loaded.Primary.Imports.push_back("external_items.ttrpg");
    loaded.ByStem["external_items"] = TParser(Tokenize("TItem Imported { Size = 9; }")).Parse();
    auto instance = TParser(Tokenize("TLoadout ImportedLoadout { Main = Imported; }")).Parse().Instances.front();
    loaded.Primary.Instances.push_back(std::move(instance));
    EXPECT_NE(Implementation(loaded).find(".Main = CreateImported()"), std::string::npos);
    Rejects("TActor Actor {} class TExample { query int Value() { let Copy = Actor; return 1; } }", "named GameEntity factories");
}

TEST(SourceCompilerTest, DetectsNamedFactoryCyclesIncludingDefaults) {
    Rejects("TItem First { Name = Second.Name; } TItem Second { Name = First.Name; }", "cyclic named value construction");
    Rejects("class TValue { int Number = Default.Number; } TValue Default {}", "cyclic named value construction");
    Rejects("class TValue { TItem Item = Default; } TItem Default { Size = Example.Item.Size; } TValue Example {}", "cyclic named value construction");
    EXPECT_NO_THROW(Implementation(Rules("class TValue { int Number = Default.Number; } TValue Default { Number = 7; } TValue Other {}")));
    Rejects("TItem Item {} TItem Item {}", "duplicate named value");
}

TEST(SourceCompilerTest, DetectsConstructionCyclesThroughResolvedQueryBodies) {
    Rejects("class TLoop { int Value = @self.DefaultValue(); query int DefaultValue() { return Loop.Value; } } TLoop Loop {}", "cyclic named value construction");
    Rejects("class TLoop { int Value = @self.DefaultValue(); query int DefaultValue() { return @self.ReadDefault(); } query int ReadDefault() { return Loop.Value; } } TLoop Loop {}", "cyclic named value construction");
    Rejects("class TCaller { int Value = TReader {}.ReadValue(); } class TReader { query int ReadValue() { return Caller.Value; } } TCaller Caller {}", "cyclic named value construction");
    EXPECT_NO_THROW(Implementation(Rules("class TCaller { int Value = TReader {}.ReadValue(2); } class TReader { query int ReadValue(int Caller) { return Caller; } } TCaller Caller {}")));
    EXPECT_NO_THROW(Implementation(Rules("enum EChoice { Caller } class TCaller { EChoice Value = TReader {}.ReadValue(); } class TReader { query EChoice ReadValue() { return Caller; } } TCaller Caller {}")));
    EXPECT_NO_THROW(Implementation(Rules("class TLoop { int Value = @self.DefaultValue(); query int DefaultValue() { return Loop.Value; } } TLoop Loop { Value = 7; }")));
}

TEST(SourceCompilerTest, AcceptsOrdinaryClassParametersAndPureCollectionLoops) {
    const auto loaded = Rules(R"(
        class TProfile {
            collection<TItem> Items;
            int Total = @self.TotalSize();
            query int Read(TLater Other) { return Other.Value; }
            query int TotalSize() {
                let Result = 0;
                for Item in @self.Items { Result += Item.Size; }
                return Result;
            }
            method int Sum(TItem Item) { return Item.Size; }
        }
        class TLater { int Value; }
    )");
    const auto header = Header(loaded);
    EXPECT_LT(header.find("class TLater;"), header.find("class TProfile {"));
    EXPECT_NE(header.find("int Read(TLater arg_Other) const;"), std::string::npos);
    const auto implementation = Implementation(loaded);
    EXPECT_NE(implementation.find("auto local_Item = *event_value_0;"), std::string::npos);
    EXPECT_NE(implementation.find("local_Result = TRuleInteger::Add(local_Result, (static_cast<const TItem&>(local_Item)).Size())"), std::string::npos);
    Rejects("class TProfile { method void Change(TItem Item) { Item.Grow(1); } }", "stable persistent receiver");
    Rejects("class TProfile { method void Change(TItem Item) { Item.Size = 1; } }", "assignment target");
    Rejects("class TProfile { query int Read(TActor Actor) { return Actor.Level; } }", "ordinary class value types");
}

TEST(SourceCompilerTest, RejectsInvalidSourcesAndCollectionMutation) {
    Rejects("action A for TActor using Missing Item {}", "ordinary class type");
    Rejects("action A for TActor using Resource Item {}", "ordinary class type");
    Rejects("action A for TActor using TActor Item {}", "ordinary class type");
    Rejects("action A for TActor using TItem Item { Item.Grow(1); }", "stable persistent receiver");
    Rejects("action A for TActor using TItem Item { Item.Size = 1; }", "assignment target");
    Rejects("action A for TActor { for Item in @self.Inventory { choose TActor Target; } }", "only at the top level");
    Rejects("action A for TActor { for Item in @self.Inventory { Item.Grow(1); } }", "stable persistent receiver");
}

TEST(SourceCompilerTest, RejectsMalformedMapsSetsAndPayloadAccess) {
    Rejects("class TProfile { map<ECategory, int> Values = { Missing = 1; }; }", "unknown map key");
    Rejects("class TProfile { map<ECategory, int> Values = { Simple = 1; Simple = 2; }; }", "duplicate map key");
    Rejects("class TProfile { map<ECategory, int> Values = { Simple = true; }; }", "expected 'int'");
    Rejects("class TProfile { set<TTrait> Traits = [Agile, Agile]; }", "duplicate variant alternative");
    Rejects("class TProfile { set<TTrait> Traits = [Fatal { Missing = 1; }]; }", "unknown variant initializer field");
    Rejects("class TProfile { query int Read(TItem Item) { return Item.Traits.get(Fatal, { Die = true; }).Die; } }", "expected 'EDie'");
    Rejects("class TProfile { query int Read(TItem Item) { return Item.Traits.get(Thrown, { Range = 0; }).Missing; } }", "unknown variant payload field");
    Rejects("class TProfile { query bool Read(TItem Item) { return Item.Traits.has(Unknown); } }", "unknown alternative");
    Rejects("class TTraitFatal {}", "generated variant type name collision");
    Rejects("enum ETraitKind { Other }", "generated variant type name collision");
}

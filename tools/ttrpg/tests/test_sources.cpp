#include <gtest/gtest.h>

#include "compile_helpers.h"

#include <ttrpg/parser.h>
#include <ttrpg/program.h>

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
    return LoadRules("sources", kTypes + source);
}

std::string Implementation(const TLoadedSchemas& loaded) {
    return EmitImplText(loaded, "sources");
}

void Rejects(const std::string& source, const std::string& diagnostic) {
    ExpectRejected(source, diagnostic, [&] { Implementation(Rules(source)); });
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
    const auto callback = body.rfind("auto source_Item = ActionSource<::TActor, ::TItem>(Context)");
    ASSERT_NE(callback, std::string::npos);
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
    loaded.Primary.Imports.push_back("external_items.ttrpg");
    loaded.ByStem["external_items"] = TParser(Tokenize("TItem Imported { Size = 9; }")).Parse();
    auto instance = TParser(Tokenize("TLoadout ImportedLoadout { Main = Imported; }")).Parse().Instances.front();
    loaded.Primary.Instances.push_back(std::move(instance));
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
    Rejects("action A for TActor { for Item in @self.Inventory { choose TActor Target; } }", "synchronous scope");
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
    Rejects("class TTraitFatal {}", "conflicts with existing symbol");
    Rejects("enum ETraitKind { Other }", "conflicts with existing symbol");
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

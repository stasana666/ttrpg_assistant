#include <gtest/gtest.h>

#include "compile_helpers.h"

#include <ttrpg/program.h>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace {

TLoadedSchemas Rules(const std::string& source) {
    return LoadRules("source_choices", R"(
        enum EResult { Success, Failure }
        class TItem { string Name = "Item"; int Size = 2; method void SetSize(int Value) { @self.Size = Value; } query TItem Identity() { return @self; } }
        class TOther { int Value = 1; }
        class TInventory { collection<TItem> Items; }
        GameEntity TActor { int Level = 1; Resource Effort = 3; collection<TItem> Items; collection<TItem> NaturalItems; TInventory Inventory; }
    )" + source);
}

std::string Emit(const TLoadedSchemas& loaded) {
    return EmitImplText(loaded, "source_choices");
}

void Rejects(const std::string& source, const std::string& diagnostic) {
    ExpectRejected(source, diagnostic, [&] { (void)Emit(Rules(source)); });
}

}

TEST(SourceChoiceCompilerTest, UsesLiveCandidatesAndReadonlySnapshotFrameSlots) {
    const auto loaded = Rules(R"(
        action Use for TActor {
            cost @self.Effort = 1;
            choose TItem Item from @self.Items where Item.Size > 0;
            choose int Added from [1, 2];
            @self.Level = Item.Size + Added;
        }
    )");
    const auto action = CompileAction(loaded.Primary.Abilities.front(), loaded);
    const auto& choice = action.Flow.Blocks.front().Choice;
    EXPECT_TRUE(choice.Source);
    EXPECT_FALSE(choice.Entity);
    EXPECT_EQ(choice.CppType, "TGeneratedSourceBinding<::TItem>");
    EXPECT_EQ(choice.PredicateCppType, "TGeneratedSourceCandidate<::TItem>");
}

TEST(SourceChoiceCompilerTest, CombinesStoredSourcesAndTracksNestedAndOtherEntityPaths) {
    const auto loaded = Rules(R"(
        action Use for TActor {
            choose TActor Owner;
            choose TItem Item from sources(Owner.Items, @self.Inventory.Items, @self.NaturalItems);
            @self.Level = Item.Size;
        }
    )");
    const auto action = CompileAction(loaded.Primary.Abilities.front(), loaded);
    const auto block = std::find_if(action.Flow.Blocks.begin(), action.Flow.Blocks.end(), [](const auto& value) { return value.Choice.Source; });
    ASSERT_NE(block, action.Flow.Blocks.end());
    EXPECT_TRUE(action.Requirement.size() > 0);
}

TEST(SourceChoiceCompilerTest, PreservesAliasesAndForksActivityInputsWithoutChangingParentContext) {
    const auto loaded = Rules(R"(
        activity int UseItem for TActor(TItem Item) { choose int Added from [1, 2]; return Item.Size + Added; }
        action Use for TActor {
            choose TItem Item from @self.Items;
            let Alias = Item;
            let Value = UseItem(Alias);
            @self.Level = Value;
        }
    )");
    const auto action = CompileAction(loaded.Primary.Abilities.front(), loaded);
    ASSERT_EQ(action.Flow.Slots.size(), 3u);
    EXPECT_EQ(action.Flow.Slots[1].CppType, "TGeneratedSourceBinding<::TItem>");
    const auto invoke = std::find_if(action.Flow.Blocks.begin(), action.Flow.Blocks.end(), [](const auto& value) {
        return value.Exit == TActionFlowBlock::EExit::Invoke;
    });
    ASSERT_NE(invoke, action.Flow.Blocks.end());
    EXPECT_EQ(invoke->Invocation.Arguments.front(), "local_Alias.Read()");
    EXPECT_EQ(invoke->Invocation.InputValidators.size(), 2u);
}

TEST(SourceChoiceCompilerTest, ReleasesScopedSourcesBeforeLaterActivityCalls) {
    const auto loaded = Rules(R"(
        activity TOther Report for TActor() { return TOther {}; }
        action Use for TActor {
            if (true) { choose TItem Item from @self.Items; @self.Level += Item.Size; }
            let Value = Report();
            @self.Level += Value.Value;
        }
    )");
    const auto action = CompileAction(loaded.Primary.Abilities.front(), loaded);
    const auto invoke = std::find_if(action.Flow.Blocks.begin(), action.Flow.Blocks.end(), [](const auto& value) {
        return value.Exit == TActionFlowBlock::EExit::Invoke;
    });
    ASSERT_NE(invoke, action.Flow.Blocks.end());
    EXPECT_TRUE(invoke->Invocation.InputValidators.empty());
    EXPECT_TRUE(invoke->LiveSlots.empty());
}

TEST(SourceChoiceCompilerTest, RejectsTemporaryCopiedMixedOrDerivedSourceCollections) {
    Rejects("action Use for TActor { choose TItem Item from TInventory {}.Items; }", "stored collection path");
    Rejects("action Use for TActor { let Copy = @self.Items; choose TItem Item from Copy; }", "stored collection path");
    Rejects("action Use for TActor { choose TItem Item from [TItem {}]; }", "list literals");
    Rejects("action Use for TActor { choose TOther Item from @self.Items; }", "stored collection of 'TOther'");
    Rejects("action Use for TActor { choose TItem Item from sources(); }", "at least one");
    Rejects("action Use for TActor { choose TItem Item from sources(@self.Items, 2); }", "stored collection");
    Rejects("action Use for TActor { let Items = sources(@self.Items); }", "only supported");
    Rejects("class TDerived { derive collection<TItem> Items = []; }", "derive");
}

TEST(SourceChoiceCompilerTest, RejectsMutableBindingsAndRawSourceEscapes) {
    Rejects("action Use for TActor { choose TItem Item from @self.Items; Item.Size = 9; }", "assignment target");
    Rejects("action Use for TActor { choose TItem Item from @self.Items; Item.SetSize(9); }", "persistent");
    Rejects("action Use for TActor { choose TItem Item from @self.Items; let Alias = Item; Alias = TItem {}; }", "cannot be rebound");
    Rejects("action Use for TActor { let Value = TItem {}; choose TItem Item from @self.Items; Value = Item; }", "ordinary value storage");
    Rejects("activity TItem GetItem for TActor() { choose TItem Item from @self.Items; return Item; }", "cannot be returned");
    Rejects("activity TItem GetItem for TActor() { choose TItem Item from @self.Items; let Alias = Item; return Alias; }", "cannot be returned");
}

TEST(SourceChoiceCompilerTest, RejectsClassResultsWithDirectAndInheritedSelectedInputs) {
    const std::string functions = R"(
        activity TItem Identity for TActor(TItem Item) { return Item; }
        activity int Nested for TActor(TItem Item) { let Copy = Identity(Item); return Copy.Size; }
    )";
    Rejects(functions + "action Use for TActor { choose TItem Item from @self.Items; let Value = Identity(Item); }", "class-valued activity 'Identity'");
    Rejects(functions + "action Use for TActor { choose TItem Item from @self.Items; let Alias = Item; let Value = Nested(Alias); }", "class-valued activity 'Identity'");
    Rejects("activity TOther Report for TActor() { return TOther {}; } action Use for TActor { choose TItem Item from @self.Items; let Value = Report(); }", "class-valued activity");
}

TEST(SourceChoiceCompilerTest, KeepsOrdinaryAndGrantedActivityValueResultsAvailable) {
    const auto loaded = Rules(R"(
        activity TItem Identity for TActor(TItem Item) { return Item; }
        action Ordinary for TActor { let Value = Identity(TItem {}); @self.Level = Value.Size; }
        action Granted for TActor using TItem Item { let Alias = Item; let Value = Identity(Alias); @self.Level = Value.Size; }
    )");
    EXPECT_NO_THROW(Emit(loaded));
    const auto granted = CompileAction(loaded.Primary.Abilities.back(), loaded);
    EXPECT_EQ(granted.Flow.Slots.front().CppType, "TGeneratedSourceBinding<::TItem>");
}

TEST(SourceChoiceCompilerTest, AllowsComputedPrimitiveEnumAndPositionResultsWithSelectedInputs) {
    const auto loaded = Rules(R"(
        activity int Size for TActor(TItem Item) { return Item.Size; }
        activity EResult Outcome for TActor(TItem Item) { return Success; }
        activity Position Point for TActor(TItem Item) { return Position { X = Item.Size; Y = 0; }; }
        action Use for TActor {
            choose TItem Item from @self.Items;
            let SizeValue = Size(Item);
            let OutcomeValue = Outcome(Item);
            let PositionValue = Point(Item);
        }
    )");
    EXPECT_NO_THROW(Emit(loaded));
}

TEST(SourceChoiceCompilerTest, ReservesSourcesAndDoesNotSpeculateAfterExecutableStatements) {
    Rejects("event int sources for TActor();", "conflicts");
    Rejects("activity int sources for TActor() { return 1; }", "conflicts");
    const auto loaded = Rules("action Use for TActor { let Value = roll(1, 6); choose TItem Item from @self.Items; }");
    EXPECT_TRUE(CompileAction(loaded.Primary.Abilities.front(), loaded).Requirement.empty());
}

TEST(SourceChoiceCompilerTest, RejectsClassIdentityQueriesAndAggregateEscapes) {
    const std::string query = "class TCopy { query TItem Copy(TItem Value) { return Value; } } ";
    Rejects("action Use for TActor { let Outside = TItem {}; if (true) { choose TItem Item from @self.Items; Outside = Item.Identity(); } }", "cannot be materialized");
    Rejects(query + "action Use for TActor { choose TItem Item from @self.Items; let Copy = TCopy {}.Copy(Item); }", "cannot be materialized");
    Rejects("activity TItem GetItem for TActor() { choose TItem Item from @self.Items; return Item.Identity(); }", "cannot be materialized");
    Rejects("class TBox { TItem Item; } action Use for TActor { choose TItem Item from @self.Items; let Box = TBox { Item = Item; }; }", "cannot be materialized");
    Rejects("action Use for TActor { choose TItem Item from @self.Items; let Box = TInventory { Items = [Item]; }; }", "cannot be materialized");
    Rejects("class TMap { map<EResult, TItem> Items; } action Use for TActor { choose TItem Item from @self.Items; let Map = TMap { Items = { Success = Item; }; }; }", "cannot be materialized");
    Rejects("activity int Size for TActor(TItem Item) { return Item.Size; } action Use for TActor { choose TItem Item from @self.Items; let Amount = Size(Item.Identity()); }", "cannot be materialized");
}

TEST(SourceChoiceCompilerTest, AllowsPrimitiveSnapshotsFromTransientSelectedClassReads) {
    const auto loaded = Rules(R"(
        action Use for TActor {
            let Amount = 0;
            if (true) {
                choose TItem Item from @self.Items;
                Amount = Item.Identity().Size;
            }
            let Report = TOther { Value = Amount; };
            @self.Level = Report.Value;
        }
    )");
    EXPECT_NO_THROW(Emit(loaded));
}

TEST(SourceChoiceCompilerTest, RejectsClassContainersAndEventSnapshotsWithSelectedOrigins) {
    Rejects(R"(
        class TBundle { collection<TItem> Items; }
        GameEntity TOwner { collection<TBundle> Bundles; }
        action Use for TOwner { choose TBundle Bundle from @self.Bundles; let Copy = Bundle.Items; }
    )", "cannot be materialized");
    Rejects(R"(
        event TItem CopyItems for TActor(TItem Item);
        action Use for TActor { choose TItem Item from @self.Items; let Copies = CopyItems(@self, Item); }
    )", "cannot be materialized");
}

TEST(SourceChoiceCompilerTest, RejectsSelectedValuesPassedThroughMutatingMethodParameters) {
    const std::string storage = R"(
        variant TPayload { Held { TItem Item; } }
        GameEntity TStorage {
            collection<TItem> Items;
            set<TPayload> Stored;
            method void Keep(TItem Value) { @self.Stored.set(Held { Item = Value; }); }
        }
    )";
    Rejects(storage + "action Store for TStorage { choose TItem Item from @self.Items; @self.Keep(Item); }", "cannot be passed to mutating methods");
    Rejects(storage + "action Store for TStorage { choose TItem Item from @self.Items; @self.Keep(Item.Identity()); }", "cannot be passed to mutating methods");
    EXPECT_NO_THROW(Emit(Rules(storage + "action Store for TStorage { @self.Keep(TItem { Size = 4; }); }")));
}

TEST(SourceChoiceCompilerTest, AuditsMutatingSinksThroughNestedActivitiesWithoutTaintingPrimitiveReconstruction) {
    const std::string storage = R"(
        variant TPayload { Held { TItem Item; } }
        GameEntity TStorage {
            collection<TItem> Items;
            set<TPayload> Stored;
            method void Keep(TItem Value) { @self.Stored.set(Held { Item = Value; }); }
        }
    )";
    Rejects(storage + R"(
        activity void Store for TStorage(TItem Value) { @self.Keep(Value); }
        action Use for TStorage { choose TItem Item from @self.Items; Store(Item); }
    )", "cannot be passed to mutating methods");
    Rejects(storage + R"(
        activity void Store for TStorage(TItem Value) { let Copy = Value.Identity(); @self.Keep(Copy); }
        activity void Wrap for TStorage(TItem Value) { Store(Value); }
        action Use for TStorage { choose TItem Item from @self.Items; Wrap(Item); }
    )", "cannot be passed to mutating methods");
    Rejects(storage + R"(
        activity void Store for TStorage(TItem Value) {
            let Copy = TItem {};
            if (true) { Copy = Value; }
            @self.Keep(Copy);
        }
        action Use for TStorage { choose TItem Item from @self.Items; Store(Item); }
    )", "cannot be assigned during activity input validation");
    Rejects(storage + R"(
        activity void Store for TStorage(TItem Value) {
            let Copy = TItem {};
            let Remaining = 2;
            while (Remaining > 0) {
                @self.Keep(Copy);
                Copy = Value;
                Remaining -= 1;
            }
        }
        action Use for TStorage { choose TItem Item from @self.Items; Store(Item); }
    )", "cannot be assigned during activity input validation");
    EXPECT_NO_THROW(Emit(Rules(storage + R"(
        activity void Store for TStorage(TItem Value) { @self.Keep(TItem { Size = Value.Size; }); }
        action Use for TStorage { choose TItem Item from @self.Items; Store(Item); }
    )")));
}

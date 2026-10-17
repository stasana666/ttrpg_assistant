#include <gtest/gtest.h>

#include "compile_helpers.h"

#include <ttrpg/program.h>
#include <ttrpg/records.h>

#include <stdexcept>
#include <string>

namespace {

const std::string kRecord = "record HitResult for TActor using TItem Item(TActor Target, EOutcome Outcome); ";

const std::string kTypes = R"(
    enum EOutcome { Miss, Hit }
    class TItem { string Name; int Damage = 3; }
    class TReport { int Amount = 0; }
    GameEntity TActor {
        int Value = 10;
        Resource Effort = 3;
        collection<TItem> Items;
        method void Hurt(int Amount) { @self.Value -= Amount; }
        query bool Alive() { return @self.Value > 0; }
    }
)";


TLoadedSchemas Rules(const std::string& source) {
    return LoadRules("records", kTypes + source);
}

std::string Header(const TLoadedSchemas& loaded) {
    return EmitHeaderText(loaded, "records");
}

}

TEST(RecordCompilerTest, ParsesTypedRecordAndTerminalPublication) {
    const auto loaded = Rules(kRecord + R"(
        action Attack for TActor using TItem Item {
            cost @self.Effort = 1;
            choose TActor Target;
            Target.Hurt(Item.Damage);
            publish HitResult(Item, Target, Hit);
        }
    )");
    ASSERT_EQ(loaded.Primary.Records.size(), 1u);
    const auto& record = loaded.Primary.Records.front();
    EXPECT_EQ(record.SourceName, "Item");
    EXPECT_EQ(record.Parameters.at(0).Name, "Target");
    const auto compiled = CompileAction(loaded.Primary.Abilities.front(), loaded);
    EXPECT_TRUE(compiled.Flow.HasActionTicket);
    std::size_t publications = 0;
    for (const auto& block : compiled.Flow.Blocks) {
        if (block.Exit != TActionFlowBlock::EExit::Publish) continue;
        ++publications;
    }
    EXPECT_EQ(publications, 1u);
}

TEST(RecordCompilerTest, BindsAfterBeforePaymentAndRetainsInputsAcrossChoices) {
    const auto loaded = Rules(kRecord + R"(
        activity void Damage for TActor(TActor Target, int Amount) { choose int Extra from [1, 2]; Target.Hurt(Amount + Extra); }
        action Follow for TActor using TItem Item after HitResult Previous {
            cost @self.Effort = 1;
            requires same_source(Item, Previous.Item) && Previous.Outcome == Hit && Previous.Target.Alive();
            let Target = Previous.Target;
            let Source = Previous.Item;
            choose int Amount from [1, 2];
            Damage(Target, Amount + Source.Damage);
        }
    )");
    const auto compiled = CompileAction(loaded.Primary.Abilities.front(), loaded);
    EXPECT_FALSE(compiled.Requirement.empty());
    EXPECT_FALSE(compiled.Flow.HasActionTicket);
    ASSERT_FALSE(compiled.Flow.Initializers.empty());
    bool validatesChildSource = false;
    for (const auto& block : compiled.Flow.Blocks) if (block.Exit == TActionFlowBlock::EExit::Invoke) {
        validatesChildSource = !block.Invocation.InputValidators.empty();
    }
    EXPECT_TRUE(validatesChildSource);
}

TEST(RecordCompilerTest, RejectsInvalidOwnersTypesNamesAndSubscriptions) {
    for (const auto& source : {
        "record R for TItem using TItem Item();",
        "record R for TActor using TActor Item();",
        "record R for TActor using TItem Item(Resource Amount);",
        "record R for TActor using TItem Item(int Item);",
        "record R for TActor using TItem TStored();",
        "record R for TActor using TItem Item(); record R for TActor using TItem Item();",
        "record R for TActor using TItem Item(); class TRecordR {}",
        "record R for TActor using TItem Item(); action TRecordR;",
        "record TActor for TActor using TItem Item();",
        "record R for TActor using TItem Item(); class TRecordedEntity {}",
        "class TActionTicket {} action Work for TActor {}",
        "action same_source;",
        "action Follow for TActor after Unknown Previous {}",
        "record R for TActor using TItem Item(); GameEntity TOther {} action Follow for TOther after R Previous {}",
        "record R for TActor using TItem Item(); action Follow for TActor using TItem Previous after R Previous {}",
        "record R for TActor using TItem Item(); trigger int Opening(int Pending); reaction Follow for TActor on Opening after R Previous { return Pending; }"
    }) {
        SCOPED_TRACE(source);
        EXPECT_THROW(Header(Rules(source)), std::runtime_error);
    }
}

TEST(RecordCompilerTest, RejectsPublicationOutsideTerminalRootAction) {
    for (const auto& source : {
        "activity void Work for TActor(TItem Item,TActor Target) { publish HitResult(Item,Target,Hit); }",
        "action Work for TActor using TItem Item { publish HitResult(Item,@self,Hit); @self.Hurt(1); }",
        "action Work for TActor using TItem Item { if (true) { publish HitResult(Item,@self,Hit); } }",
        "action Work for TActor using TItem Item { while (true) { publish HitResult(Item,@self,Hit); } }",
        "action Work for TActor using TItem Item { for Value in [1] { publish HitResult(Item,@self,Hit); } }",
        "class TBad { method void Work() { publish HitResult(); } }",
        "trigger int Opening(int Pending); reaction Work for TActor using TItem Item on Opening { publish HitResult(Item,@self,Hit); }"
    }) {
        SCOPED_TRACE(source);
        EXPECT_THROW(Header(Rules(kRecord + source)), std::runtime_error);
    }
}

TEST(RecordCompilerTest, RejectsRawSourceCopiesEffectsAndRecordEscapes) {
    for (const auto& source : {
        "action Work for TActor { publish HitResult(TItem {},@self,Hit); }",
        "action Work for TActor using TItem Item { publish HitResult(Item,@self); }",
        "action Work for TActor using TItem Item { publish HitResult(Item,1,Hit); }",
        "action Work for TActor using TItem Item { publish HitResult(Item,@self,roll(1,6)); }",
        "action Work for TActor using TItem Item { let Result=HitResult(Item,@self,Hit); }",
        "action Follow for TActor after HitResult Previous { let Copy=Previous; }",
        "action Follow for TActor after HitResult Previous { Previous.Outcome=Hit; }",
        "action Follow for TActor after HitResult Previous { requires Previous.Target.Hurt(1); }",
        "action Follow for TActor after HitResult Previous { require same_source(TItem {},Previous.Item); }",
        "action Follow for TActor after HitResult Previous { require same_source(Previous.Item); }",
        "action Follow for TActor after HitResult Previous { Previous.Missing.Hurt(1); }"
    }) {
        SCOPED_TRACE(source);
        EXPECT_THROW(Header(Rules(kRecord + source)), std::runtime_error);
    }
}

TEST(RecordCompilerTest, RejectsHistoricalSourceClassEscapeThroughActivity) {
    const auto loaded = Rules(kRecord + R"(
        activity TItem Copy for TActor(TItem Item) { return Item; }
        action Follow for TActor after HitResult Previous { let Item = Copy(Previous.Item); }
    )");
    EXPECT_THROW(Header(loaded), std::runtime_error);
}

TEST(RecordCompilerTest, AcceptsHistoricalSourceAliasesInPreflight) {
    const auto loaded = Rules(kRecord + R"(
        action Follow for TActor using TItem Item after HitResult Previous {
            let Current = Item;
            let Earlier = Previous.Item;
            choose TActor Target where same_source(Current,Earlier) && Target == Previous.Target;
        }
    )");
    EXPECT_NO_THROW(Header(loaded));
}


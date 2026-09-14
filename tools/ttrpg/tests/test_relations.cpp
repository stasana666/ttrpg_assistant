#include <gtest/gtest.h>

#include <ttrpg/emit.h>
#include <ttrpg/parser.h>
#include <ttrpg/program.h>
#include <ttrpg/relations.h>

#include <sstream>
#include <stdexcept>
#include <string>

namespace {

const std::string kTypes = R"(
    enum EGrade { Low, High }
    class TValue { int Amount = 0; query int Total() { return @self.Amount; } }
    class TItem { int Size = 2; }
    GameEntity TActor { int Value = 1; Resource Effort = 3; collection<TItem> Items; }
    GameEntity TObject { int Value = 0; }
)";

const std::string kRelation = "relation bool Mark for TActor(TActor Source); ";

TLoadedSchemas Rules(const std::string& source) {
    TLoadedSchemas loaded;
    loaded.PrimaryStem = "relations";
    loaded.Primary = TParser(Tokenize(kTypes + source)).Parse();
    RegisterModuleSymbols(loaded.PrimaryStem, loaded.Primary, loaded.SymbolTable);
    return loaded;
}

std::string Header(const TLoadedSchemas& loaded) {
    std::ostringstream output;
    EmitHeader(output, loaded, "relations.ttrpg", "relations.h");
    return output.str();
}

std::string Implementation(const TLoadedSchemas& loaded) {
    std::ostringstream output;
    EmitImpl(output, loaded, "relations.ttrpg", "relations.h");
    return output.str();
}

std::string Join(const std::vector<std::string>& lines) {
    std::string result;
    for (const auto& line : lines) result += line + "\n";
    return result;
}

void Rejects(const std::string& source, const std::string& diagnostic) {
    try {
        (void)Header(Rules(source));
        FAIL() << "expected rejection: " << source;
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find(diagnostic), std::string::npos) << error.what();
    }
}

}

TEST(RelationCompilerTest, ParsesDistinctEntityKeyAndTypedPayloadWithCanonicalTag) {
    const auto loaded = Rules("relation TValue Detail for TActor(TObject Source);");
    ASSERT_EQ(loaded.Primary.Relations.size(), 1u);
    const auto& relation = loaded.Primary.Relations.front();
    EXPECT_EQ(relation.ValueType, "TValue");
    EXPECT_EQ(relation.OwnerType, "TActor");
    ASSERT_EQ(relation.Parameters.size(), 1u);
    EXPECT_EQ(relation.Parameters.front().TypeName, "TObject");
    EXPECT_EQ(relation.Parameters.front().Name, "Source");
    const auto header = Header(loaded);
    const auto code = Implementation(loaded);
    EXPECT_NE(header.find("using TValue = ::TValue;"), std::string::npos);
    EXPECT_NE(header.find("using TKey = ::TObject;"), std::string::npos);
    EXPECT_NE(header.find("kId = \"relations.Detail\""), std::string::npos);
    EXPECT_NE(header.find("kKeyTypeName = \"TObject\""), std::string::npos);
    EXPECT_NE(code.find("AddOwnedObject(node, \"value\", value, context)"), std::string::npos);
    EXPECT_NE(code.find("rules.Relations.Register<::TRelationDetail>()"), std::string::npos);
}

TEST(RelationCompilerTest, PreflightReadsRelationsWithoutExecutingTimedOrPermanentWrites) {
    const auto loaded = Rules(kRelation + R"(
        action Remember for TActor {
            cost @self.Effort = 1;
            let Duration = minutes(10);
            choose TActor Target where Target != @self && !Mark.has(Target, @self);
            Mark.set(Target, @self, true, Duration);
            Mark.set(@self, Target, false);
            require Mark.get(Target, @self, false);
            Mark.erase(@self, Target);
        }
    )");
    const auto compiled = CompileAction(loaded.Primary.Abilities.front(), loaded);
    const auto requirement = Join(compiled.Requirement);
    EXPECT_NE(requirement.find("::HasRelation<::TRelationMark>(world"), std::string::npos);
    EXPECT_NE(requirement.find("::GameMinutes(10)"), std::string::npos);
    EXPECT_EQ(requirement.find("SetRelation"), std::string::npos);
    EXPECT_EQ(requirement.find("EraseRelation"), std::string::npos);
    const auto code = Implementation(loaded);
    EXPECT_NE(code.find("std::optional<::TGameDuration>"), std::string::npos);
    EXPECT_NE(code.find("::SetRelation<::TRelationMark>(Context"), std::string::npos);
    EXPECT_NE(code.find("::GetRelation<::TRelationMark>(world"), std::string::npos);
    EXPECT_NE(code.find("::EraseRelation<::TRelationMark>(Context"), std::string::npos);
}

TEST(RelationCompilerTest, DoesNotSpeculatePastRelationMutation) {
    for (const auto& before : {"Mark.set(@self,@self,true,seconds(1));", "Mark.set(@self,@self,false);", "Mark.erase(@self,@self);"}) {
        const auto loaded = Rules(kRelation + "action Work for TActor { " + before + " choose TActor Target; }");
        EXPECT_TRUE(CompileAction(loaded.Primary.Abilities.front(), loaded).Requirement.empty());
    }
}

TEST(RelationCompilerTest, ReturnsIndependentTypedPayloadCopiesAndContextualFallbacks) {
    const auto loaded = Rules(R"(
        relation TValue Detail for TActor(TObject Source);
        relation EGrade Grade for TActor(TObject Source);
        relation Position Origin for TActor(TObject Source);
        relation string Label for TActor(TObject Source);
        action Remember for TActor {
            choose TObject Target;
            Detail.set(@self, Target, { Amount = 4; });
            let Copy = Detail.get(@self, Target, { Amount = 1; });
            @self.Value = Copy.Total();
            require Grade.get(@self, Target, Low) == Low;
            Grade.set(@self, Target, High, seconds(0));
            Origin.set(@self, Target, position(@self));
            let Previous = Origin.get(@self, Target, Position { X = 1; Y = 2; });
            @self.Value = Previous.X;
            Label.set(@self, Target, "seen");
        }
    )");
    EXPECT_NO_THROW(Header(loaded));
    const auto code = Implementation(loaded);
    EXPECT_NE(code.find("::EGrade::Low"), std::string::npos);
    EXPECT_NE(code.find("::GameSeconds(0)"), std::string::npos);
    EXPECT_NE(code.find("AddValueField(node, \"value\", value)"), std::string::npos);
}

TEST(RelationCompilerTest, SupportsPureHooksAndReadonlyKeysInActionWrites) {
    const auto loaded = Rules(kRelation + R"(
        event bool IsMarked for TActor(TActor Target);
        feature Watchful { on IsMarked { return Mark.has(Target, @self); } }
        activity void Remember for TActor(TActor Target) { Mark.set(Target,@self,true,seconds(2)); }
        trigger bool Opening(bool Pending, TActor Target);
        reaction Clear for TActor on Opening { Mark.erase(Target,@self); return Pending; }
        action Survey for TActor {
            for Source in entities(TActor) {
                let Alias = Source;
                Mark.set(@self, Alias, false);
            }
        }
    )");
    EXPECT_NO_THROW(Implementation(loaded));
}

TEST(RelationCompilerTest, RejectsMutationThroughReadonlyAliasesAndPureScopes) {
    Rejects(kRelation + "action A for TActor { for Target in entities(TActor) { let Alias=Target; Mark.set(Alias,@self,true); } }", "mutable owner");
    Rejects(kRelation + "action A for TActor { for Target in entities(TActor) { Mark.erase(Target,@self); } }", "mutable owner");
    Rejects(kRelation + "action A for TActor { requires Mark.set(@self,@self,true); }", "effectful");
    Rejects(kRelation + "action A for TActor { choose TActor Target where Mark.erase(Target,@self); }", "effectful");
    Rejects(kRelation + "event bool Check for TActor(TActor Target); feature F { on Check { Mark.set(Target,@self,true); return true; } }", "effectful");
    Rejects(kRelation + "class TQuery { query bool Read() { return Mark.has(@self,@self); } }", "world queries");
    Rejects(kRelation + "class TDefault { bool Value=Mark.has(@self,@self); }", "world queries");
    Rejects(kRelation + "TActor Named(int Value=1) { requires Mark.has(@self,@self); }", "world queries");
}

TEST(RelationCompilerTest, RejectsWrongArityEntityTypesPayloadsAndEffectfulArguments) {
    const std::vector<std::string> calls{
        "Mark.has(@self)", "Mark.get(@self,@self)", "Mark.get(@self,@self,1)",
        "Mark.set(@self,@self)", "Mark.set(@self,@self,true,1)", "Mark.set(@self,@self,true,seconds(1),2)",
        "Mark.erase(@self)", "Mark.erase(@self,@self,false)", "Mark.missing(@self,@self)",
        "Mark.has(@self,1)", "Mark.has(1,@self)", "Mark(@self,@self)",
        "Mark.set(@self,@self,true,seconds(roll(1,6)))"
    };
    for (const auto& call : calls) {
        SCOPED_TRACE(call);
        EXPECT_THROW(Header(Rules(kRelation + "action A for TActor { " + call + "; }")), std::runtime_error);
    }
    Rejects("relation int Mark for TActor(TObject Source); action A for TActor { Mark.set(@self,@self,1); }", "expected 'TObject'");
    Rejects("relation int Mark for TActor(TActor Source); action A for TActor { Mark.set(@self,@self,roll(1,6)); }", "effectful");
    Rejects("relation int Mark for TActor(TActor Source); action A for TActor { let Value=Mark.get(@self,@self,roll(1,6)); }", "effectful");
}

TEST(RelationCompilerTest, RejectsSourceClassEscapeIncludingInheritedActivityArguments) {
    const auto prefix = "relation TItem Cache for TActor(TActor Source); ";
    Rejects(prefix + std::string("action A for TActor { choose TItem Item from @self.Items; Cache.set(@self,@self,Item); }"), "selected source");
    Rejects(prefix + std::string("action A for TActor { choose TItem Item from @self.Items; let Copy=Cache.get(@self,@self,Item); }"), "selected source");
    Rejects(prefix + std::string("activity void Store for TActor(TItem Item) { Cache.set(@self,@self,Item); } action A for TActor { choose TItem Item from @self.Items; Store(Item); }"), "selected source");
}

TEST(RelationCompilerTest, DurationIsPureOpaqueLocalAndCannotBecomePersistentState) {
    const auto loaded = Rules("class TTimer { query int Probe() { let Delay=minutes(2); return 1; } } action A for TActor { let Delay=seconds(-1); choose int Value from [1,2]; Delay=minutes(Value); }");
    EXPECT_NO_THROW(Header(loaded));
    EXPECT_NE(Implementation(loaded).find("pf2e_engine/common/game_time.h"), std::string::npos);
    for (const auto& source : {
        "class TTimer { Duration Delay; }", "class TTimer { query Duration Delay() { return seconds(1); } }",
        "activity Duration Delay for TActor() { return seconds(1); }",
        "activity void Delay for TActor(Duration Value) {}", "relation Duration Delay for TActor(TActor Source);",
        "action A for TActor { let Delay=seconds(); }", "action A for TActor { let Delay=minutes(true); }",
        "action A for TActor { let Delay=seconds(1,2); }", "action A for TActor { let Delay=seconds(1)+seconds(2); }",
        "action A for TActor { @self.Value=seconds(1); }", "action A for TActor { let Delay=seconds(1); let Value=Delay.Seconds(); }"
    }) {
        SCOPED_TRACE(source);
        EXPECT_THROW(Header(Rules(source)), std::runtime_error);
    }
}

TEST(RelationCompilerTest, RejectsInvalidDeclarationsAndGeneratedNameCollisions) {
    for (const auto& source : {
        "relation bool R for TActor();", "relation bool R for TActor(TActor A,TActor B);",
        "relation bool R for TValue(TActor A);", "relation bool R for TActor(int A);",
        "relation bool R for TActor(TValue A);", "relation TActor R for TActor(TActor A);",
        "relation Resource R for TActor(TActor A);", "relation Feature R for TActor(TActor A);",
        "relation bool R for TActor(TActor A); relation bool R for TActor(TActor A);",
        "relation bool TActor for TActor(TActor A);", "relation bool R for TActor(TActor A); class TRelationR {}",
        "relation bool R for TActor(TActor A); feature TRelationR;", "relation bool seconds for TActor(TActor A);",
        "class Duration {}", "class TGameDuration {}", "feature minutes;", "class GameSeconds {}",
        "relation bool R for TActor(TActor A); relation bool TRelationR for TActor(TActor A);",
        "class TBad { TActor Actor; } relation TBad R for TActor(TActor A);"
    }) {
        SCOPED_TRACE(source);
        EXPECT_THROW(Header(Rules(source)), std::runtime_error);
    }
}

TEST(RelationCompilerTest, EnforcesModuleVisibilityAndEscapesCanonicalIds) {
    auto loaded = Rules("action Use for TActor { require Hidden.has(@self,@self); }");
    loaded.ByStem.emplace("hidden", TParser(Tokenize("relation bool Hidden for TActor(TActor Source);")).Parse());
    EXPECT_THROW(Header(loaded), std::runtime_error);
    loaded.Primary.Imports.push_back("hidden.ttrpg");
    EXPECT_NO_THROW(Header(loaded));
    EXPECT_NE(Implementation(loaded).find("RegisterHiddenRules(rules)"), std::string::npos);
    auto escaped = Rules(kRelation);
    escaped.PrimaryStem = "name\"\\line";
    EXPECT_NE(Header(escaped).find("name\\\"\\\\line.Mark"), std::string::npos);
}

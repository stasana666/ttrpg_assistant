#include <pf2e_engine/actions/generated_source.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>

#include "../test_lib/mock_dice_roller.h"
#include "../test_lib/mock_interaction_system.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

struct TTestSource {
    int Value;
    std::vector<TTestSource> Children;

    template <class TVisitor>
    void VisitAbilitySources(TVisitor& visitor, const TAbilitySourcePath& path = {}) const
    {
        visitor(*this, path);
        for (std::size_t i = 0; i < Children.size(); ++i) {
            Children[i].VisitAbilitySources(visitor, path.Field("children").Entry(i));
        }
    }
};

template <class TSource>
class TCountingSourceFinder : public TAbilitySourceFinder<TSource> {
public:
    explicit TCountingSourceFinder(TAbilitySourcePath path)
        : TAbilitySourceFinder<TSource>(std::move(path))
    {
    }

    template <class TValue>
    void operator()(const TValue& value, const TAbilitySourcePath& path)
    {
        ++visits_;
        TAbilitySourceFinder<TSource>::operator()(value, path);
    }

    std::size_t Visits() const
    {
        return visits_;
    }

private:
    std::size_t visits_ = 0;
};

TIdCollection<TFeatureSelection> Training()
{
    TIdCollection<TFeatureSelection> result;
    result.Add(TFeatureSelection("test.Training"));
    return result;
}

TAction::TBoundResources Costs()
{
    return {{
        .Key = "hands",
        .Count = 1,
        .Read = [](const TPlayer& player) -> const TResource& {
            return BindActionEntity<TCreature>(player).Read().Hands();
        },
        .Write = [](TPlayer& player) {
            return BindActionEntity<TCreature>(player).Project(
                [](TCreature& creature) { return creature.Hands(); });
        },
    }};
}

std::shared_ptr<const TAbilityExecution> UsingExecution(TAction::FBody body)
{
    return std::make_shared<const TAbilityExecution>(TAbilityExecution{
        .Action = std::make_shared<const TAction>(std::move(body), Costs(), "using source"),
        .Trigger = {},
        .Requirement = [](const TPlayer& actor, const TAbilitySourcePath& source, const auto*) {
            return RequiredSource<TCreature, TWeapon>(actor, source);
        },
        .Prepare = [](auto ctx, TPlayer& actor, const TGrantedAbility& grant) {
            PrepareActionSource<TCreature, TWeapon>(ctx, actor, grant);
        },
    });
}

class TSourceBindingRuntimeTest : public ::testing::Test {
protected:
    void Start(std::shared_ptr<const TAbilityExecution> execution)
    {
        rules_ = std::make_shared<TAbilityRules>();
        rules_->Catalog.Register({"test.Using", EAbilityKind::Action, {}});
        rules_->Catalog.Register({"test.Training", EAbilityKind::Feature, {"test.Using"}});
        rules_->Executions.Register("test.Using", std::move(execution));
        nlohmann::json map = {{"x_size", 8}, {"y_size", 8}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, rules_);
        transformator_ = std::make_unique<TTransformator>(io_);
    }

    TPlayer& Add(bool weapon_source = true)
    {
        const int id = static_cast<int>(creatures_.size());
        TIdCollection<TWeapon> weapons;
        if (weapon_source) {
            weapons.RestoreEntry(TId<TWeapon>{7}, TWeapon::Create({
                .Name = "test source", .BaseDiceSize = 6, .Features = Training(),
            }));
            weapons.RestoreNextId(TId<TWeapon>{9});
        }
        creatures_.push_back(TCreature::Create({
            .Level = 1,
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 10}), .Current = 10,
            }),
            .Weapons = std::move(weapons),
            .Features = weapon_source ? TIdCollection<TFeatureSelection>{} : Training(),
            .Hands = TResource(3),
        }));
        rng_.ExpectCall(20, 10);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id}, TPlayerId{id},
                                   "Actor " + std::to_string(id), ""), {id, 0});
        return *battle_->GetIfPlayers([id](const TPlayer* player) {
            return player->GetId() == id;
        }).at(0);
    }

    std::shared_ptr<TActionContext> Context()
    {
        return std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = transformator_.get(),
            .io_system = &io_, .scheduler = nullptr, .effect_manager = nullptr,
        });
    }

    TAbilityCollector Collect(const TPlayer& actor)
    {
        return CollectAbilities(*actor.GetCreature(), rules_->Catalog);
    }

    TAbilityOption Option(const TPlayer& actor)
    {
        return EnumerateActions(actor.GetId(), Collect(actor), rules_->Executions, actor).at(0);
    }

    EAbilityExecutionResult Execute(TPlayer& actor, const std::shared_ptr<TActionContext>& context)
    {
        return ExecuteAbility(Option(actor), context, actor, Collect(actor), rules_->Executions);
    }

    void ReplaceWeapons(TPlayer& actor, int die, bool present = true)
    {
        TIdCollection<TWeapon> replacement;
        if (present) {
            replacement.RestoreEntry(TId<TWeapon>{7}, TWeapon::Create({
                .Name = "replacement", .BaseDiceSize = die, .Features = Training(),
            }));
        }
        replacement.RestoreNextId(TId<TWeapon>{9});
        auto context = transformator_->ExecutionContext();
        context.AssignGuarded(actor.GetCreature()->Weapons(), std::move(replacement),
                               TGuarded<TCreature>(*actor.GetCreature()), "weapons");
    }

    void TearDown() override
    {
        rng_.Verify();
    }

    TMockRng rng_;
    TMockInteractionSystem io_;
    std::deque<TCreature> creatures_;
    std::shared_ptr<TAbilityRules> rules_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> transformator_;
};

}

TEST(AbilitySourceResolverTest, NearestTypedAncestorUsesPathSegmentsAndTraversalOrderIsIrrelevant)
{
    const TTestSource source{1, {{2, {{3, {}}}}, {4, {}}}};
    const auto branch = TAbilitySourcePath{}.Field("children").Entry(0);
    const auto nested = branch.Field("children").Entry(0);
    const auto requested = nested.Field("features").Entry(8);
    const auto found = FindAbilitySource<TTestSource>(source, requested);
    ASSERT_TRUE(found);
    EXPECT_EQ(found->Value->Value, 3);
    EXPECT_EQ(found->Path, nested);
    TAbilitySourceFinder<TTestSource> reverse(requested);
    reverse(source.Children[0].Children[0], nested);
    reverse(source.Children[0], branch);
    reverse(source.Children[1], TAbilitySourcePath{}.Field("children").Entry(10));
    reverse(source, {});
    ASSERT_TRUE(reverse.Get());
    EXPECT_EQ(reverse.Get()->Value->Value, 3);
    EXPECT_FALSE(FindAbilitySource<TWeapon>(source, requested));
}

TEST(AbilitySourceResolverTest, AmbiguousProviderPathsAreRejected)
{
    const TTestSource one{1, {}};
    const TTestSource two{2, {}};
    const auto path = TAbilitySourcePath{}.Field("nested");
    TAbilitySourceFinder<TTestSource> finder(path.Field("features"));
    finder(one, {});
    finder(two, path);
    EXPECT_NO_THROW(finder(two, path));
    EXPECT_THROW(finder(two, {}), std::invalid_argument);
}

TEST(AbilityExecutionRegistrationTest, SharedExecutionsHaveStableIdentityWithoutWeakeningConflicts)
{
    TAbilityExecutions executions;
    const auto execution = UsingExecution([](auto) {});
    executions.Register("test.Using", execution);
    executions.Register("test.Using", execution);
    EXPECT_EQ(&executions.Require("test.Using"), execution.get());
    EXPECT_THROW(executions.Register("test.Using",
        std::make_shared<const TAbilityExecution>(*execution)), std::invalid_argument);
    EXPECT_THROW(executions.Register("test.Using", execution->Action), std::invalid_argument);
    EXPECT_THROW(executions.Register("test.Using", std::shared_ptr<const TAbilityExecution>{}),
                 std::invalid_argument);
    EXPECT_THROW(executions.Register("test.Using", nullptr), std::invalid_argument);
    EXPECT_THROW(executions.Register("", execution), std::invalid_argument);
    auto invalid = *execution;
    invalid.Trigger = ETrigger::COUNT;
    EXPECT_THROW(executions.Register("test.Invalid",
        std::make_shared<const TAbilityExecution>(invalid)), std::invalid_argument);
    invalid = *execution;
    invalid.Action.reset();
    EXPECT_THROW(executions.Register("test.Invalid",
        std::make_shared<const TAbilityExecution>(invalid)), std::invalid_argument);
    executions.Register("test.Plain", execution->Action);
    EXPECT_NO_THROW(executions.Register("test.Plain", execution->Action));
    EXPECT_THROW(executions.Register("test.Required", nullptr, {},
        [](const auto&, const auto&, const auto*) { return true; }), std::invalid_argument);
    const auto required = [](const auto&, const auto&, const auto*) { return true; };
    executions.Register("test.Required", execution->Action, {}, required);
    EXPECT_THROW(executions.Register("test.Required", execution->Action, {}, required),
                 std::invalid_argument);
    auto only_preparation = *execution;
    only_preparation.Requirement = {};
    const auto prepared = std::make_shared<const TAbilityExecution>(only_preparation);
    executions.Register("test.Prepared", prepared);
    EXPECT_NO_THROW(executions.Register("test.Prepared", prepared));
    EXPECT_THROW(executions.Register("test.Prepared",
        std::make_shared<const TAbilityExecution>(only_preparation)), std::invalid_argument);
    const auto copied = executions;
    executions = {};
    EXPECT_EQ(&copied.Require("test.Using"), execution.get());
}

TEST_F(TSourceBindingRuntimeTest, GeneratedLookupPrunesProvidersOutsideTheGrantPath)
{
    Start(UsingExecution([](auto) {}));
    const auto& actor = Add();
    const auto option = Option(actor);
    TCountingSourceFinder<TWeapon> finder(option.Source);

    actor.GetCreature()->VisitAbilitySources(finder);

    ASSERT_TRUE(finder.Get().has_value());
    EXPECT_EQ(finder.Get()->Path, TAbilitySourcePath{}.Field("weapons").Entry(7));
    EXPECT_EQ(finder.Get()->Value->BaseDiceSize(), 6);
    EXPECT_EQ(finder.Visits(), 2u);
}

TEST_F(TSourceBindingRuntimeTest, MissingTypedProviderIsExcludedAndCannotConsumeAForgedOption)
{
    Start(UsingExecution([](auto) { FAIL() << "body must not run"; }));
    auto& actor = Add(false);
    const auto collector = Collect(actor);
    ASSERT_EQ(collector.Grants().size(), 1);
    EXPECT_TRUE(EnumerateActions(actor.GetId(), collector, rules_->Executions, actor).empty());
    const auto& grant = collector.Grants()[0];
    const TAbilityOption option{
        .ActorId = actor.GetId(), .ActorIdentity = &actor, .DefinitionId = grant.DefinitionId,
        .Source = grant.Source, .Provenance = grant.Provenance,
        .Action = rules_->Executions.Require(grant.DefinitionId).Action, .Trigger = {},
    };
    EXPECT_EQ(ExecuteAbility(option, Context(), actor, collector, rules_->Executions),
               EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    EXPECT_THROW((PrepareActionSource<TCreature, TWeapon>(Context(), actor, grant)),
                 TAbilityUnavailable);
}

TEST_F(TSourceBindingRuntimeTest, PreparationFailureLeavesPaymentAndJournalUntouched)
{
    Start(std::make_shared<const TAbilityExecution>(TAbilityExecution{
        .Action = std::make_shared<const TAction>([](auto) { FAIL(); }, Costs(), "fails preparation"),
        .Trigger = {}, .Requirement = {},
        .Prepare = [](auto, auto&, const auto&) { throw std::runtime_error("cannot snapshot"); },
    }));
    auto& actor = Add();
    TAstContext before_context;
    const auto before = transformator_->GetAst(before_context);
    EXPECT_THROW(Execute(actor, Context()), std::runtime_error);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 3);
    TAstContext after_context;
    EXPECT_EQ(transformator_->GetAst(after_context), before);
}

TEST_F(TSourceBindingRuntimeTest, PreparationCapturesResourceBeforePaymentAndRunsOnceAcrossReplay)
{
    int prepared = 0;
    int observed = 0;
    Start(std::make_shared<const TAbilityExecution>(TAbilityExecution{
        .Action = std::make_shared<const TAction>([&](auto ctx) {
            const auto source = ActionSource<TCreature, TResource>(ctx);
            throw TSavepointStackUnwind(ctx->transformator->CurrentState(), [&, source]() {
                observed = source.Read().Count();
            });
        }, Costs(), "snapshot payment"),
        .Trigger = {}, .Requirement = {},
        .Prepare = [&](auto ctx, auto& actor, const auto&) {
            ++prepared;
            ctx->generated_source = std::make_shared<const TGeneratedSourceView>(
                typeid(TCreature), typeid(TResource),
                std::make_shared<const TResource>(std::as_const(*actor.GetCreature()).Hands()),
                []() {});
        },
    }));
    auto& actor = Add();
    std::optional<TSavepointStackUnwind> suspended;
    try {
        Execute(actor, Context());
        FAIL();
    } catch (const TSavepointStackUnwind& savepoint) {
        suspended = savepoint;
    }
    ASSERT_TRUE(suspended);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
    suspended->Resume();
    EXPECT_EQ(observed, 3);
    suspended->Revert(*transformator_);
    suspended->Resume();
    EXPECT_EQ(observed, 3);
    EXPECT_EQ(prepared, 1);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
}

TEST_F(TSourceBindingRuntimeTest, SnapshotSurvivesProviderReplacementAndContextDestruction)
{
    std::optional<TGeneratedSourceBinding<TWeapon>> snapshot;
    Start(UsingExecution([&](auto ctx) { snapshot = ActionSource<TCreature, TWeapon>(ctx); }));
    auto& actor = Add();
    auto context = Context();
    const std::weak_ptr<TActionContext> weak = context;
    EXPECT_EQ(Execute(actor, context), EAbilityExecutionResult::Applied);
    ASSERT_TRUE(snapshot);
    EXPECT_EQ(snapshot->Read().BaseDiceSize(), 6);
    EXPECT_THROW((ActionSource<TCreature, TClass>(context)), std::invalid_argument);
    EXPECT_THROW((ActionSource<TWeapon, TWeapon>(context)), std::invalid_argument);
    context.reset();
    EXPECT_TRUE(weak.expired());
    ReplaceWeapons(actor, 12);
    EXPECT_EQ(snapshot->Read().BaseDiceSize(), 6);
    const auto previous = *snapshot;
    EXPECT_EQ(Execute(actor, Context()), EAbilityExecutionResult::Applied);
    EXPECT_EQ(snapshot->Read().BaseDiceSize(), 12);
    EXPECT_EQ(previous.Read().BaseDiceSize(), 6);
}

TEST_F(TSourceBindingRuntimeTest, RemovedProviderAndDeadActorInvalidateReadsWithoutRefund)
{
    std::optional<TGeneratedSourceBinding<TWeapon>> snapshot;
    Start(UsingExecution([&](auto ctx) { snapshot = ActionSource<TCreature, TWeapon>(ctx); }));
    auto& actor = Add();
    EXPECT_EQ(Execute(actor, Context()), EAbilityExecutionResult::Applied);
    ASSERT_TRUE(snapshot);
    const auto after_payment = transformator_->CurrentState();
    ReplaceWeapons(actor, 0, false);
    EXPECT_THROW(snapshot->Read(), TAbilityUnavailable);
    transformator_->Undo(after_payment);
    EXPECT_EQ(snapshot->Read().BaseDiceSize(), 6);
    transformator_->DealDamage(&actor, 10);
    EXPECT_THROW(snapshot->Validate(), TAbilityUnavailable);
    transformator_->Undo(after_payment);
    EXPECT_NO_THROW(snapshot->Validate());
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
}

TEST_F(TSourceBindingRuntimeTest, EveryResumeChecksSourceBeforeAnUnrelatedNativeCallback)
{
    int resumed = 0;
    Start(UsingExecution([&](auto ctx) {
        throw TSavepointStackUnwind(ctx->transformator->CurrentState(), [&]() { ++resumed; });
    }));
    auto& actor = Add();
    std::optional<TSavepointStackUnwind> suspended;
    try {
        Execute(actor, Context());
        FAIL();
    } catch (const TSavepointStackUnwind& savepoint) {
        suspended = savepoint;
    }
    ASSERT_TRUE(suspended);
    suspended->Resume();
    EXPECT_EQ(resumed, 1);
    suspended->Revert(*transformator_);
    ReplaceWeapons(actor, 0, false);
    EXPECT_THROW(suspended->Resume(), TAbilityUnavailable);
    EXPECT_EQ(resumed, 1);
    suspended->Revert(*transformator_);
    suspended->Resume();
    EXPECT_EQ(resumed, 2);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 2);
}

TEST_F(TSourceBindingRuntimeTest, ReusingContextForAnUnpreparedActionClearsItsOldSource)
{
    Start(UsingExecution([](auto) {}));
    auto& actor = Add();
    const auto context = Context();
    EXPECT_EQ(Execute(actor, context), EAbilityExecutionResult::Applied);
    ASSERT_TRUE(context->generated_source);
    ReplaceWeapons(actor, 0, false);
    TAbilityCollector collector(rules_->Catalog);
    collector.Grant("test.Using", {}, "plain adapter");
    TAbilityExecutions executions;
    executions.Register("test.Using", std::make_shared<const TAction>([](auto ctx) {
        EXPECT_FALSE(ctx->generated_source);
        EXPECT_THROW((ActionSource<TCreature, TWeapon>(ctx)), std::invalid_argument);
    }, Costs(), "unprepared"));
    const auto option = EnumerateActions(actor.GetId(), collector, executions, actor).at(0);
    EXPECT_EQ(ExecuteAbility(option, context, actor, collector, executions),
               EAbilityExecutionResult::Applied);
    EXPECT_EQ(actor.GetCreature()->Hands()->Count(), 1);
}

TEST_F(TSourceBindingRuntimeTest, PreparationRejectsAPlayerOutsideTheAnchoredBattle)
{
    Start(UsingExecution([](auto) {}));
    auto& actor = Add();
    TPlayer foreign(actor.GetCreature(), TPlayerTeam{1}, TPlayerId{actor.GetId()}, "Foreign", "");
    const auto collector = Collect(actor);
    TAstContext before_context;
    const auto before = transformator_->GetAst(before_context);
    EXPECT_THROW((PrepareActionSource<TCreature, TWeapon>(Context(), foreign,
                                                          collector.Grants().at(0))),
                 TAbilityUnavailable);
    TAstContext after_context;
    EXPECT_EQ(transformator_->GetAst(after_context), before);
}

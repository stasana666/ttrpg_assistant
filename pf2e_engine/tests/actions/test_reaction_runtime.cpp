#include <pf2e_engine/actions/reaction_window.h>
#include <pf2e_engine/actions/generated_source.h>
#include <pf2e_engine/actions/generated_movement.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/inventory/weapon.h>

#include "../test_lib/mock_dice_roller.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

struct TPending {
    int Amount = 10;
    bool Disrupted = false;
    friend bool operator==(const TPending&, const TPending&) = default;
};

struct TBeforeChange {
    using TState = TPending;
    using TArguments = std::tuple<TGeneratedEntityBinding<TCreature>>;
    static constexpr std::string_view kId = "native.BeforeChange";
};

struct TInnerChange : TBeforeChange {
    static constexpr std::string_view kId = "native.InnerChange";
};

struct TConflictingChange : TBeforeChange {};

using TExecution = TReactionExecution<TBeforeChange>;
using TResult = TReactionWindowResult<TPending>;

TBoundResourceCost Cost(int count, bool actions = false)
{
    return {
        .Key = actions ? "self.actions" : "self.reactions", .Count = count,
        .Read = [actions](const TPlayer& actor) -> const TResource& {
            return actions ? actor.GetCreature()->Actions() : actor.GetCreature()->Reactions();
        },
        .Write = [actions](TPlayer& actor) {
            return actions ? actor.GetCreature()->Actions() : actor.GetCreature()->Reactions();
        },
    };
}

TExecution Definition(TExecution::FRun body = [](auto, TPending value, auto, auto) { return value; })
{
    return {
        .OwnerType = typeid(TCreature), .Name = "Counter", .LimitKey = "counter",
        .Payment = TResourcePayment({Cost(1)}), .Requirement = {}, .Prepare = {},
        .Run = std::move(body),
    };
}

class TReactionIo final : public IInteractionSystem {
public:
    void Expect(int actor, std::string kind, std::string label, bool suspend = false)
    {
        choices_.push_back({actor, std::move(kind), std::move(label), suspend});
    }

    void RequestChoice(int actor, TAlternatives alternatives, TState state,
                       std::function<void(size_t)> receive) override
    {
        if (choices_.empty()) {
            throw std::logic_error("unexpected reaction choice");
        }
        const auto choice = choices_.front();
        choices_.pop_front();
        EXPECT_EQ(actor, choice.Actor);
        EXPECT_EQ(alternatives.Kind(), choice.Kind);
        ++requests_;
        if (alternatives.Kind() == "reaction") {
            last_reactions_.clear();
            for (std::size_t index = 0; index < alternatives.Size(); ++index) {
                last_reactions_.push_back(alternatives[index].name);
            }
        }
        if (choice.Suspend) {
            throw TSavepointStackUnwind(state, [this, alternatives, choice, receive]() {
                const auto label = resume_label_.value_or(choice.Label);
                resume_label_.reset();
                receive(Index(alternatives, label));
            });
        }
        ++depth_;
        max_depth_ = std::max(depth_, max_depth_);
        try {
            receive(Index(alternatives, choice.Label));
        } catch (...) {
            --depth_;
            throw;
        }
        --depth_;
    }

    void ResumeAs(std::string label) { resume_label_ = std::move(label); }
    int Requests() const { return requests_; }
    int MaxDepth() const { return max_depth_; }
    const std::vector<std::string>& LastReactions() const { return last_reactions_; }
    void Verify() const { EXPECT_TRUE(choices_.empty()); }
    void HandleReactionTrigger(const TTriggerContext&, const TState&) override { ++legacy_calls_; }
    int LegacyCalls() const { return legacy_calls_; }
    std::ostream& GameLog() override { return log_; }
    std::ostream& DevLog() override { return log_; }

protected:
    size_t ChooseAlternativeIndex(int actor, const TAlternatives& alternatives) override
    {
        if (choices_.empty()) {
            throw std::logic_error("unexpected synchronous choice");
        }
        const auto choice = choices_.front();
        choices_.pop_front();
        EXPECT_EQ(actor, choice.Actor);
        EXPECT_EQ(alternatives.Kind(), choice.Kind);
        return Index(alternatives, choice.Label);
    }

private:
    static std::size_t Index(const TAlternatives& alternatives, const std::string& label)
    {
        for (std::size_t index = 0; index < alternatives.Size(); ++index) {
            if (alternatives[index].name == label) {
                return index;
            }
        }
        throw std::logic_error("missing reaction choice: " + label);
    }

    struct TChoice {
        int Actor;
        std::string Kind;
        std::string Label;
        bool Suspend;
    };
    std::deque<TChoice> choices_;
    std::optional<std::string> resume_label_;
    std::vector<std::string> last_reactions_;
    std::ostringstream log_;
    int requests_ = 0;
    int depth_ = 0;
    int max_depth_ = 0;
    int legacy_calls_ = 0;
};

TSavepointStackUnwind Pause(const std::function<void()>& work)
{
    try {
        work();
    } catch (const TSavepointStackUnwind& point) {
        return point;
    }
    throw std::logic_error("expected a suspended reaction");
}

TPending Adjust(std::shared_ptr<TActionContext> context, TPending value,
                TBeforeChange::TArguments arguments, int remaining, TExecution::FResume on_resume)
{
    const auto actor = GeneratedActionSelf(context);
    ValidateActionEntity(context, std::get<0>(arguments).View());
    if (remaining == 0) {
        return value;
    }
    auto next = [context, actor, value, arguments, remaining, on_resume](int amount) {
        ValidateActionActorBinding(context, actor);
        ValidateActionEntity(context, std::get<0>(arguments).View());
        context->transformator->AddResource(ActionSelf<TCreature>(context).Project(
            [](auto& creature) { return creature.Hands(); }), 1);
        auto changed = value;
        changed.Amount -= amount;
        return Adjust(context, changed, arguments, remaining - 1, on_resume);
    };
    const auto amount = SelectActionValue<int>(context, {1, 2}, {},
        [next, on_resume](int selected) { on_resume(next(selected)); });
    return next(amount);
}

class TReactionRuntimeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        rules_ = std::make_shared<TAbilityRules>();
        rules_->Catalog.Register({"native.Parent", EAbilityKind::Action, {}});
        rules_->Catalog.Register({"native.ParentFeature", EAbilityKind::Feature, {"native.Parent"}});
        rules_->Catalog.Register({"native.Counter", EAbilityKind::Reaction, {}});
        rules_->Catalog.Register({"native.CounterFeature", EAbilityKind::Feature, {"native.Counter"}});
        rules_->Executions.Register("native.Parent", std::make_shared<TAction>(
            [](auto) {}, TBoundResources{}, "Parent"));
    }

    void Register(TExecution execution)
    {
        rules_->Reactions.Register<TBeforeChange>("native.Counter",
            std::make_shared<const TExecution>(std::move(execution)));
    }

    void Start(bool second_reactor = false, bool weapons = false)
    {
        nlohmann::json map = {{"x_size", 4}, {"y_size", 3}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, rules_);
        Add(0, {"native.ParentFeature"}, 20);
        Add(1, weapons ? std::vector<std::string>{} : std::vector<std::string>{"native.CounterFeature"}, 10, weapons);
        Add(2, second_reactor ? std::vector<std::string>{"native.CounterFeature"} : std::vector<std::string>{}, 5);
        journal_ = std::make_unique<TTransformator>(io_);
        context_ = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = journal_.get(),
            .io_system = &io_, .scheduler = nullptr, .effect_manager = nullptr,
        });
        context_->game_object_registry = std::make_shared<TGameObjectRegistry>();
        context_->game_object_registry->Add(TGameObjectIdManager::Instance().Register("self"), &Player(0));
        context_->validate_ability = [this] {
            const auto abilities = CollectAbilities(Creature(0), rules_->Catalog);
            return std::ranges::any_of(abilities.Grants(), [](const auto& grant) {
                return grant.DefinitionId == "native.Parent";
            });
        };
    }

    void Add(int id, const std::vector<std::string>& selections, int initiative, bool weapons = false)
    {
        TIdCollection<TFeatureSelection> features;
        for (const auto& selection : selections) {
            features.Add(TFeatureSelection(selection));
        }
        TIdCollection<TWeapon> equipment;
        if (weapons) {
            for (int index = 0; index < 2; ++index) {
                TIdCollection<TFeatureSelection> source_features;
                source_features.Add(TFeatureSelection("native.CounterFeature"));
                equipment.Add(TWeapon::Create({.Name = "Weapon", .Features = std::move(source_features)}));
            }
        }
        creatures_.push_back(TCreature::Create({
            .Hitpoints = THitPoints::Create({.Maximum = Statistic::Create({.Base = 100}), .Current = 100}),
            .Weapons = std::move(equipment),
            .Features = std::move(features), .Actions = TResource(3), .Reactions = TResource(4),
            .Hands = TResource(0),
        }));
        rng_.ExpectCall(20, initiative);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id == 0 ? 0 : 1},
                                  TPlayerId{id}, "Actor " + std::to_string(id), ""), {id, 0});
    }

    TPlayer& Player(int id)
    {
        return *battle_->GetIfPlayers([id](const TPlayer* player) { return player->GetId() == id; }).at(0);
    }
    TCreature& Creature(int id) { return *Player(id).GetCreature(); }
    TBeforeChange::TArguments Arguments() { return {BindActionEntity<TCreature>(Player(0))}; }
    TResult Open(std::function<void(TResult)> resume = [](auto) {}, TReactionScope scope = {})
    {
        return OpenReactionWindow<TBeforeChange>(context_, std::move(scope), TPending{}, Arguments(), std::move(resume));
    }
    void RemoveFeatures(int id, TTransformator& journal)
    {
        auto execution = journal.ExecutionContext();
        execution.AssignGuarded(Creature(id).Features(), TIdCollection<TFeatureSelection>{},
                                TGuarded<TCreature>(Creature(id)), "features");
    }
    TAstNode Snapshot()
    {
        TAstContext context;
        auto result = TAstNode::MakeObject("reaction test");
        AddOwnedObject(result, "battle", *battle_, context);
        AddOwnedObject(result, "journal", *journal_, context);
        return result;
    }
    void TearDown() override { io_.Verify(); rng_.Verify(); }

    std::shared_ptr<TAbilityRules> rules_;
    TReactionIo io_;
    TMockRng rng_;
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> journal_;
    std::shared_ptr<TActionContext> context_;
};

TEST(ReactionRegistryTest, ImmutableRegistrationRejectsConflictsAndSupportsMultipleTriggers)
{
    TReactionRegistry registry;
    const auto execution = std::make_shared<const TExecution>(Definition());
    registry.Register<TBeforeChange>("reaction", execution);
    EXPECT_NO_THROW(registry.Register<TBeforeChange>("reaction", execution));
    EXPECT_EQ(registry.Find<TBeforeChange>("reaction"), execution);
    EXPECT_TRUE(registry.Contains("reaction"));
    EXPECT_FALSE(registry.Contains("missing"));
    EXPECT_THROW(registry.Register<TBeforeChange>("reaction", std::make_shared<const TExecution>(Definition())), std::invalid_argument);
    EXPECT_THROW(registry.Register<TBeforeChange>("", execution), std::invalid_argument);
    EXPECT_THROW(registry.Register<TBeforeChange>("missing", nullptr), std::invalid_argument);
    EXPECT_THROW(registry.Find<TConflictingChange>("reaction"), std::invalid_argument);
    TReactionExecution<TInnerChange> inner{
        .OwnerType = typeid(TCreature), .Name = "Inner", .LimitKey = {}, .Payment = TResourcePayment{},
        .Requirement = {}, .Prepare = {}, .Run = [](auto, auto value, auto, auto) { return value; },
    };
    registry.Register<TInnerChange>("reaction", std::make_shared<const TReactionExecution<TInnerChange>>(std::move(inner)));
    EXPECT_NE(registry.Find<TInnerChange>("reaction"), nullptr);
}

TEST_F(TReactionRuntimeTest, TypedGrantsValidateWithoutActionsAndLegacyEnumerationStaysSeparate)
{
    Register(Definition());
    Start();
    EXPECT_EQ(rules_->Executions.Find("native.Counter"), nullptr);
    const auto abilities = CollectAbilities(Creature(1), rules_->Catalog);
    EXPECT_EQ(RequireGrantImplementation(*rules_, abilities.Grants().at(0)), EAbilityImplementation::TypedReaction);
    EXPECT_TRUE(battle_->AvailableReactions({ETrigger::OnMove, &Player(0)}).empty());
    auto invalid = *rules_;
    invalid.Executions.Register("native.Counter", std::make_shared<TAction>([](auto) {}, TBoundResources{}, "Legacy"), ETrigger::OnMove);
    EXPECT_THROW(RequireGrantImplementation(invalid, abilities.Grants().at(0)), std::invalid_argument);
    invalid.Reactions = {};
    EXPECT_EQ(RequireGrantImplementation(invalid, abilities.Grants().at(0)), EAbilityImplementation::LegacyReaction);
    EXPECT_EQ(EnumerateReactions(1, abilities, invalid, Player(1), {ETrigger::OnMove, &Player(0)}).size(), 1);
    invalid.Executions = {};
    EXPECT_THROW(RequireGrantImplementation(invalid, abilities.Grants().at(0)), std::invalid_argument);
    invalid.Executions.Register("native.Counter", std::make_shared<TAction>([](auto) {}, TBoundResources{}, "Wrong kind"));
    EXPECT_THROW(RequireGrantImplementation(invalid, abilities.Grants().at(0)), std::invalid_argument);
}

TEST_F(TReactionRuntimeTest, RequirementsAreReadonlyAndDeclineDoesNotPayOrRun)
{
    auto execution = Definition([](auto, auto value, auto, auto) { ADD_FAILURE() << "declined reaction ran"; return value; });
    execution.Requirement = [](const auto& world, const TPlayer& actor, const auto&, const auto&, const auto& arguments) {
        EXPECT_THROW(world.template Bind<TCreature>(actor).Guard(), std::logic_error);
        EXPECT_THROW(std::get<0>(arguments).Guard(), std::logic_error);
        return true;
    };
    Register(std::move(execution));
    Start();
    const auto before = Snapshot();
    io_.Expect(1, "reaction", "Decline");
    int callbacks = 0;
    EXPECT_EQ(Open([&](auto) { ++callbacks; }).Value, TPending{});
    EXPECT_EQ(callbacks, 0);
    EXPECT_EQ(io_.LastReactions(), (std::vector<std::string>{"Decline", "Counter"}));
    EXPECT_EQ(io_.LegacyCalls(), 0);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TReactionRuntimeTest, PreparationPrecedesPaymentAndFailureDoesNotConsume)
{
    auto execution = Definition();
    bool fail = true;
    execution.Prepare = [&](auto child, TPlayer& actor, const auto&) {
        EXPECT_EQ(actor.GetCreature()->Reactions()->Count(), 4);
        EXPECT_EQ(child->transformator, journal_.get());
        EXPECT_NE(child, context_);
        if (fail) {
            throw std::runtime_error("preparation failed");
        }
    };
    Register(std::move(execution));
    Start();
    const auto before = Snapshot();
    io_.Expect(1, "reaction", "Counter");
    EXPECT_THROW(Open(), std::runtime_error);
    EXPECT_EQ(Snapshot(), before);
    fail = false;
    io_.Expect(1, "reaction", "Counter");
    Open();
    EXPECT_EQ(Creature(1).Reactions()->Count(), 3);
}

TEST_F(TReactionRuntimeTest, RepeatedSuspensionsReplayCopiedPayloadAndChargeEachReactorOnce)
{
    Register(Definition([](auto child, auto value, auto args, auto resume) {
        return Adjust(child, value, args, 2, std::move(resume));
    }));
    Start(true);
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    int callbacks = 0;
    TPending returned;
    io_.Expect(1, "reaction", "Counter");
    io_.Expect(1, "choice", "1", true);
    const auto first = Pause([&] { Open([&](auto value) { ++callbacks; returned = value.Value; }); });
    EXPECT_EQ(Creature(1).Reactions()->Count(), 3);
    io_.Expect(1, "choice", "2", true);
    const auto second = Pause([&] { first.Resume(); });
    EXPECT_EQ(Creature(1).Hands()->Count(), 1);
    io_.Expect(2, "reaction", "Counter");
    io_.Expect(2, "choice", "1");
    io_.Expect(2, "choice", "2");
    second.Resume();
    EXPECT_EQ(callbacks, 1);
    EXPECT_EQ(returned.Amount, 4);
    EXPECT_EQ(Creature(1).Hands()->Count(), 2);
    EXPECT_EQ(Creature(2).Hands()->Count(), 2);
    const auto complete = Snapshot();
    second.Revert(*journal_);
    io_.Expect(2, "reaction", "Counter");
    io_.Expect(2, "choice", "1");
    io_.Expect(2, "choice", "2");
    second.Resume();
    EXPECT_EQ(callbacks, 2);
    EXPECT_EQ(returned.Amount, 4);
    EXPECT_EQ(Snapshot(), complete);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TReactionRuntimeTest, ActorScopeLimitsDistinctWeaponSourcesAcrossSeveralWindows)
{
    auto execution = Definition();
    execution.Prepare = [](auto child, auto& actor, const auto& grant) {
        PrepareActionSource<TCreature, TWeapon>(child, actor, grant);
    };
    Register(std::move(execution));
    Start(false, true);
    io_.Expect(1, "reaction", "Counter (2)");
    auto result = Open();
    EXPECT_EQ(Creature(1).Reactions()->Count(), 3);
    result = Open([](auto) {}, result.Scope);
    result = Open([](auto) {}, result.Scope);
    EXPECT_EQ(io_.Requests(), 1);
    io_.Expect(1, "reaction", "Counter (1)");
    Open();
    EXPECT_EQ(Creature(1).Reactions()->Count(), 2);
}

TEST_F(TReactionRuntimeTest, SelectedOfferRechecksLiveGrantAndResourcesBeforePayment)
{
    Register(Definition());
    Start();
    io_.Expect(1, "reaction", "Counter", true);
    const auto offer = Pause([&] { Open(); });
    TTransformator editor(io_);
    const auto state = editor.CurrentState();
    RemoveFeatures(1, editor);
    const auto without_grant = Snapshot();
    EXPECT_THROW(offer.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Snapshot(), without_grant);
    editor.Undo(state);
    editor.ReduceResource(Creature(1).Reactions(), 4);
    EXPECT_THROW(offer.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Creature(1).Reactions()->Count(), 0);
    editor.Undo(state);
    offer.Resume();
    EXPECT_EQ(Creature(1).Reactions()->Count(), 3);
}

TEST_F(TReactionRuntimeTest, ForeignArgumentsAndEmptyContinuationFailBeforeInteraction)
{
    Register(Definition());
    Start();
    auto foreign_creature = TCreature::Create({});
    TPlayer foreign(&foreign_creature, TPlayerTeam{0}, TPlayerId{0}, "foreign", "");
    const auto before = Snapshot();
    EXPECT_THROW(OpenReactionWindow<TBeforeChange>(context_, {}, {},
        {BindActionEntity<TCreature>(foreign)}, [](auto) {}), TAbilityUnavailable);
    EXPECT_THROW(OpenReactionWindow<TBeforeChange>(context_, {}, {}, Arguments(), {}), std::invalid_argument);
    EXPECT_EQ(io_.Requests(), 0);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TReactionRuntimeTest, SuspendedBodyRejectsOwnSourceRemovalAndRestoresForReplay)
{
    Register(Definition([](auto child, auto value, auto arguments, auto resume) {
        return Adjust(child, value, arguments, 1, std::move(resume));
    }));
    Start();
    io_.Expect(1, "reaction", "Counter");
    io_.Expect(1, "choice", "2", true);
    int completed = 0;
    const auto point = Pause([&] { Open([&](auto) { ++completed; }); });
    TTransformator editor(io_);
    const auto state = editor.CurrentState();
    RemoveFeatures(1, editor);
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Creature(1).Hands()->Count(), 0);
    EXPECT_EQ(Creature(1).Reactions()->Count(), 3);
    EXPECT_EQ(completed, 0);
    editor.Undo(state);
    point.Resume();
    EXPECT_EQ(completed, 1);
    const auto completed_state = Snapshot();
    point.Revert(*journal_);
    point.Resume();
    EXPECT_EQ(completed, 2);
    EXPECT_EQ(Snapshot(), completed_state);
}

TEST_F(TReactionRuntimeTest, ParentGrantRemovalDoesNotPreventReactionFromCompleting)
{
    Register(Definition([this](auto child, auto value, auto arguments, auto resume) {
        RemoveFeatures(0, *child->transformator);
        return Adjust(child, value, arguments, 1, std::move(resume));
    }));
    Start();
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    const TAction action(TAction::TCompiledBody{[](auto parent) {
        const auto actor = GeneratedActionSelf(parent);
        auto finish = [parent, actor](TResult) { ValidateActionActorBinding(parent, actor); };
        const auto value = OpenReactionWindow<TBeforeChange>(parent, {}, {},
            {ActionSelf<TCreature>(parent)}, finish);
        finish(value);
    }}, {Cost(1, true)}, "Parent");
    io_.Expect(1, "reaction", "Counter");
    io_.Expect(1, "choice", "2", true);
    const auto point = Pause([&] { action.Apply(context_, Player(0)); });
    EXPECT_EQ(Creature(0).Actions()->Count(), 2);
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Creature(1).Hands()->Count(), 1);
    EXPECT_EQ(Creature(1).Reactions()->Count(), 3);
    const auto complete = Snapshot();
    point.Revert(*journal_);
    EXPECT_THROW(point.Resume(), TAbilityUnavailable);
    EXPECT_EQ(Snapshot(), complete);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TReactionRuntimeTest, OldOfferReplayCanSelectAnotherSourceWithoutPreviousGrantConstraints)
{
    Register(Definition([](auto child, auto value, auto arguments, auto resume) {
        return Adjust(child, value, arguments, 1, std::move(resume));
    }));
    Start(false, true);
    io_.Expect(1, "reaction", "Counter (1)", true);
    const auto offer = Pause([&] { Open(); });
    io_.Expect(1, "choice", "1");
    offer.Resume();
    EXPECT_EQ(Creature(1).Hands()->Count(), 1);
    offer.Revert(*journal_);
    TIdCollection<TWeapon> remaining;
    const auto& weapons = std::as_const(Creature(1)).Weapons();
    const auto entry = *(++weapons.begin());
    remaining.RestoreEntry(entry.Id(), *entry);
    remaining.RestoreNextId(weapons.NextId());
    auto execution = journal_->ExecutionContext();
    execution.AssignGuarded(Creature(1).Weapons(), std::move(remaining),
                            TGuarded<TCreature>(Creature(1)), "weapons");
    io_.ResumeAs("Counter (2)");
    io_.Expect(1, "choice", "2");
    offer.Resume();
    EXPECT_EQ(Creature(1).Hands()->Count(), 1);
    EXPECT_EQ(Creature(1).Reactions()->Count(), 3);
}

TEST_F(TReactionRuntimeTest, NestedDifferentTriggersReturnValuesAndActiveTriggerCyclesFail)
{
    bool cycle = false;
    TReactionExecution<TInnerChange> inner{
        .OwnerType = typeid(TCreature), .Name = "Inner", .LimitKey = {},
        .Payment = TResourcePayment({Cost(1)}), .Requirement = {}, .Prepare = {},
        .Run = [&](auto child, auto value, auto args, auto resume) {
            if (cycle) {
                return OpenReactionWindow<TBeforeChange>(child, {}, value, args,
                    [resume](auto result) { resume(result.Value); }).Value;
            }
            value.Amount -= 4;
            return value;
        },
    };
    rules_->Catalog.Register({"native.Inner", EAbilityKind::Reaction, {}});
    rules_->Catalog.Register({"native.InnerFeature", EAbilityKind::Feature, {"native.Inner"}});
    rules_->Reactions.Register<TInnerChange>("native.Inner",
        std::make_shared<const TReactionExecution<TInnerChange>>(std::move(inner)));
    Register(Definition([](auto child, auto value, auto arguments, auto resume) {
        return OpenReactionWindow<TInnerChange>(child, {}, value, arguments,
            [resume](auto result) { resume(result.Value); }).Value;
    }));
    Start();
    auto execution = journal_->ExecutionContext();
    auto features = std::as_const(Creature(2)).Features();
    features.Add(TFeatureSelection("native.InnerFeature"));
    execution.AssignGuarded(Creature(2).Features(), std::move(features), TGuarded<TCreature>(Creature(2)), "features");
    io_.Expect(1, "reaction", "Counter");
    io_.Expect(2, "reaction", "Inner", true);
    TPending result;
    const auto point = Pause([&] { Open([&](auto returned) { result = returned.Value; }); });
    point.Resume();
    EXPECT_EQ(result.Amount, 6);
    EXPECT_EQ(Creature(1).Reactions()->Count(), 3);
    EXPECT_EQ(Creature(2).Reactions()->Count(), 3);
    point.Revert(*journal_);
    cycle = true;
    EXPECT_THROW(point.Resume(), std::logic_error);
    EXPECT_EQ(Creature(1).Reactions()->Count(), 3);
    EXPECT_EQ(Creature(2).Reactions()->Count(), 3);
}

TEST_F(TReactionRuntimeTest, ImmediateBodyCannotCallItsSuspendedReturnChannel)
{
    Register(Definition([](auto, auto value, auto, auto resume) {
        resume(value);
        return value;
    }));
    Start();
    io_.Expect(1, "reaction", "Counter");
    int callbacks = 0;
    EXPECT_THROW(Open([&](auto) { ++callbacks; }), std::logic_error);
    EXPECT_EQ(callbacks, 0);
    EXPECT_EQ(Creature(1).Reactions()->Count(), 3);
}

TEST_F(TReactionRuntimeTest, ManyImmediateWindowsDoNotGrowTheContinuationStack)
{
    auto execution = Definition();
    execution.Payment = TResourcePayment{};
    execution.LimitKey.clear();
    Register(std::move(execution));
    Start();
    for (int index = 0; index < 2048; ++index) {
        io_.Expect(1, "reaction", "Counter");
    }
    for (int index = 0; index < 2048; ++index) {
        Open([](auto) { ADD_FAILURE() << "immediate window invoked continuation"; });
    }
    EXPECT_EQ(io_.Requests(), 2048);
    EXPECT_EQ(io_.MaxDepth(), 1);
    EXPECT_EQ(Creature(1).Reactions()->Count(), 4);
}

TEST_F(TReactionRuntimeTest, BattleDrainsSuspendedReactionAfterItsParentDies)
{
    Register(Definition([](auto child, auto value, auto arguments, auto resume) {
        EXPECT_EQ(std::get<0>(arguments).Read().Actions().Count(), 2);
        auto execution = child->transformator->ExecutionContext();
        execution.Invoke(std::get<0>(arguments).Project([](auto& target) { return target.Hitpoints(); }),
                          &THitPoints::DealDamage, 100);
        return Adjust(child, value, arguments, 1, std::move(resume));
    }));
    rules_->Executions = {};
    rules_->Executions.Register("native.Parent", std::make_shared<TAction>(TAction::TCompiledBody{
        [](auto parent) {
            const auto actor = GeneratedActionSelf(parent);
            auto finish = [parent, actor](TResult) { ValidateActionActorBinding(parent, actor); };
            const auto result = OpenReactionWindow<TBeforeChange>(parent, {}, {},
                {ActionSelf<TCreature>(parent)}, finish);
            finish(result);
        }}, TBoundResources{Cost(1, true)}, "Parent"));
    Start();
    io_.Expect(0, "next action", "Parent");
    io_.Expect(1, "reaction", "Counter");
    io_.Expect(1, "choice", "2", true);
    EXPECT_NO_THROW(battle_->StartBattle());
    EXPECT_FALSE(Creature(0).IsAlive());
    EXPECT_EQ(Creature(1).Hands()->Count(), 1);
    EXPECT_EQ(Creature(1).Reactions()->Count(), 3);
    EXPECT_EQ(Creature(2).Reactions()->Count(), 4);
    EXPECT_EQ(Creature(2).Actions()->Count(), 3);
    EXPECT_EQ(battle_->Winner(), std::optional<int>{1});
}

TEST_F(TReactionRuntimeTest, CompletedAndFailedBodiesRejectTheirRetainedCallbacks)
{
    TExecution::FResume late;
    bool fail = false;
    Register(Definition([&](auto, auto value, auto, auto resume) {
        late = std::move(resume);
        if (fail) {
            throw std::runtime_error("body failed");
        }
        return value;
    }));
    Start();
    int completed = 0;
    io_.Expect(1, "reaction", "Counter");
    Open([&](auto) { ++completed; });
    EXPECT_THROW(late(TPending{}), std::logic_error);
    EXPECT_EQ(completed, 0);
    fail = true;
    io_.Expect(1, "reaction", "Counter");
    EXPECT_THROW(Open([&](auto) { ++completed; }), std::runtime_error);
    EXPECT_THROW(late(TPending{}), std::logic_error);
    EXPECT_EQ(completed, 0);
    EXPECT_EQ(Creature(1).Reactions()->Count(), 2);
}

TEST_F(TReactionRuntimeTest, ReplayedOfferCanChooseAnotherDefinitionAfterOldGrantRemoval)
{
    Register(Definition());
    rules_->Catalog.Register({"native.Other", EAbilityKind::Reaction, {}});
    rules_->Catalog.Register({"native.OtherFeature", EAbilityKind::Feature, {"native.Other"}});
    auto other = Definition([](auto child, auto value, auto, auto) {
        child->transformator->AddResource(ActionSelf<TCreature>(child).Project(
            [](auto& creature) { return creature.Hands(); }), 2);
        return value;
    });
    other.Name = "Other";
    other.LimitKey = "other";
    rules_->Reactions.Register<TBeforeChange>("native.Other",
        std::make_shared<const TExecution>(std::move(other)));
    Start();
    auto features = std::as_const(Creature(1)).Features();
    const auto other_id = features.Add(TFeatureSelection("native.OtherFeature"));
    auto execution = journal_->ExecutionContext();
    execution.AssignGuarded(Creature(1).Features(), std::move(features),
                            TGuarded<TCreature>(Creature(1)), "features");
    io_.Expect(1, "reaction", "Counter", true);
    const auto offer = Pause([&] { Open(); });
    offer.Resume();
    EXPECT_EQ(Creature(1).Reactions()->Count(), 3);
    offer.Revert(*journal_);
    TIdCollection<TFeatureSelection> remaining;
    remaining.RestoreEntry(other_id, TFeatureSelection("native.OtherFeature"));
    remaining.RestoreNextId(std::as_const(Creature(1)).Features().NextId());
    execution.AssignGuarded(Creature(1).Features(), std::move(remaining),
                            TGuarded<TCreature>(Creature(1)), "features");
    io_.ResumeAs("Other");
    offer.Resume();
    EXPECT_EQ(Creature(1).Reactions()->Count(), 3);
    EXPECT_EQ(Creature(1).Hands()->Count(), 2);
}

TEST_F(TReactionRuntimeTest, ReactionMovementInvalidatesParentOriginWithoutRefundingCompletedEffects)
{
    Register(Definition([](auto child, auto value, auto arguments, auto) {
        MoveActionEntity(child, std::get<0>(arguments).View(), {0, 0}, {0, 1});
        return value;
    }));
    Start();
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    const TAction parent(TAction::TCompiledBody{[](auto context) {
        const auto actor = GeneratedActionSelf(context);
        auto finish = [context, actor](TResult) {
            ValidateActionActorBinding(context, actor);
            MoveActionEntity(context, actor, {0, 0}, {1, 1});
        };
        const auto result = OpenReactionWindow<TBeforeChange>(context, {}, {},
            {ActionSelf<TCreature>(context)}, finish);
        finish(result);
    }}, {Cost(1, true)}, "Parent");
    io_.Expect(1, "reaction", "Counter");
    EXPECT_THROW(parent.Apply(context_, Player(0)), TAbilityUnavailable);
    EXPECT_EQ(Player(0).GetPosition(), (TPosition{0, 1}));
    EXPECT_EQ(Creature(0).Actions()->Count(), 2);
    EXPECT_EQ(Creature(1).Reactions()->Count(), 3);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

}

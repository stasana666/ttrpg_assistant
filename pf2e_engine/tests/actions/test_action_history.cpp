#include <pf2e_engine/actions/generated_history.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>

#include "../test_lib/mock_dice_roller.h"
#include "../test_lib/mock_interaction_system.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace {

struct TNativeRecord {
    static constexpr std::string_view kId = "history.Result";
    using TOwner = TCreature;
    using TSource = TWeapon;
    using TArguments = std::tuple<TGeneratedEntityBinding<TCreature>, int>;

    struct TStored {
        TRecordedSource<TCreature, TWeapon> Weapon;
        TRecordedEntity<TCreature> Target;
        int Amount;
    };

    struct TInputs {
        TGeneratedSourceBinding<TWeapon> Weapon;
        TGeneratedEntityBinding<TCreature> Target;
        int Amount;
    };

    static TStored Detach(const std::shared_ptr<TActionContext>& context,
                           const TGeneratedSourceBinding<TWeapon>& source, const TArguments& arguments)
    {
        return {TRecordedSource<TCreature, TWeapon>::Capture(context, source),
                TRecordedEntity<TCreature>::Capture(context, std::get<0>(arguments)),
                std::get<1>(arguments)};
    }

    static TInputs Rebind(const TGeneratedWorldView& world, const TPlayer& actor, const TStored& value)
    {
        return {value.Weapon.ReadOnly(world, actor), value.Target.ReadOnly(world), value.Amount};
    }

    static TInputs Rebind(const std::shared_ptr<TActionContext>& context, const TStored& value)
    {
        return {value.Weapon.Bind(context), value.Target.Bind(context), value.Amount};
    }

    static TAstNode GetAst(const TStored& value, TAstContext& context)
    {
        auto node = TAstNode::MakeObject("TNativeRecord");
        AddOwnedObject(node, "weapon", value.Weapon, context);
        AddOwnedObject(node, "target", value.Target, context);
        AddValueField(node, "amount", value.Amount);
        return node;
    }
};

struct TConflictingRecord : TNativeRecord {};

TIdCollection<TFeatureSelection> Training(bool enabled = true)
{
    TIdCollection<TFeatureSelection> result;
    if (enabled) {
        result.Add(TFeatureSelection("history.Training"));
    }
    return result;
}

TIdCollection<TWeapon> Weapons(int size = 4, bool present = true, bool granted = true)
{
    TIdCollection<TWeapon> result;
    if (present) {
        result.RestoreEntry(TId<TWeapon>{7}, TWeapon::Create({
            .Name = "Recorded blade", .BaseDiceSize = size, .Features = Training(granted),
        }));
    }
    result.RestoreNextId(TId<TWeapon>{10});
    return result;
}

class TActionHistoryTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        rules_ = std::make_shared<TAbilityRules>();
        rules_->Records.Register<TNativeRecord>();
        for (const auto* name : {"history.Publish", "history.Consume", "history.Other"}) {
            rules_->Catalog.Register({name, EAbilityKind::Action, {}});
        }
        rules_->Catalog.Register({"history.Training", EAbilityKind::Feature,
                                  {"history.Publish", "history.Consume", "history.Other"}});
        Register("history.Publish", [this](const auto& context) {
            const auto ticket = CaptureActionTicket(context);
            const auto source = ActionSource<TCreature, TWeapon>(context);
            PublishActionRecord<TNativeRecord>(context, ticket, source,
                {BindActionEntity<TCreature>(*target_), amount_});
        });
        Register("history.Consume", [this](const auto& context) {
            const auto input = PreparedActionRecord<TNativeRecord>(context);
            input.Weapon.Validate(context);
            ValidateActionEntity(context, input.Target.View());
            observed_amount_ = input.Amount;
            observed_target_ = input.Target.Read().Id();
            auto execution = context->transformator->ExecutionContext();
            execution.Invoke(input.Target.Guard(), [&](TCreature& creature, TGeneratedExecutionContext&) {
                creature.ReceiveHealing(execution, input.Amount);
            });
        }, true);
        Register("history.Other", [this](const auto&) {
            if (fail_body_) {
                throw std::runtime_error("accepted body failed");
            }
        });
        battle_ = std::make_unique<TBattle>(TBattleMap(6, 6), &rng_, io_, rules_);
        journal_ = std::make_unique<TTransformator>(io_);
        actor_ = &Add();
        target_ = &Add();
    }

    void Register(std::string name, TAction::FBody body, bool previous = false)
    {
        TAction::TBoundResources cost{{
            .Key = "self.actions", .Count = 1,
            .Read = [](const TPlayer& player) -> const TResource& {
                return player.GetCreature()->Actions();
            },
            .Write = [](TPlayer& player) { return player.GetCreature()->Actions(); },
        }};
        auto action = std::make_shared<const TAction>(std::move(body), std::move(cost), name);
        rules_->Executions.Register(std::move(name), std::make_shared<const TAbilityExecution>(
            TAbilityExecution{
                .Action = std::move(action),
                .Requirement = [previous](const TGeneratedWorldView& world, const TPlayer& actor,
                                           const TAbilitySourcePath& source) {
                    return RequiredSource<TCreature, TWeapon>(actor, source)
                        && (!previous || FindActionRecord<TNativeRecord>(world, actor).has_value());
                },
                .Prepare = [previous](auto context, TPlayer& actor, const TGrantedAbility& grant) {
                    PrepareActionSource<TCreature, TWeapon>(context, actor, grant);
                    if (previous) {
                        PrepareActionRecord<TNativeRecord>(context, *context->previous_record);
                    }
                },
                .PreviousRecord = previous ? std::string(TNativeRecord::kId) : "",
            }));
    }

    TPlayer& Add()
    {
        const int id = static_cast<int>(creatures_.size());
        creatures_.push_back(TCreature::Create({
            .Level = 1,
            .Hitpoints = THitPoints::Create({
                .Maximum = Statistic::Create({.Base = 20}), .Current = 10,
            }),
            .Weapons = Weapons(), .Actions = TResource(20),
        }));
        rng_.ExpectCall(20, 10);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{id}, TPlayerId{id},
                                   "Actor " + std::to_string(id), ""), {id, 0});
        return *battle_->GetIfPlayers([id](const auto* player) { return player->GetId() == id; }).at(0);
    }

    std::shared_ptr<TActionContext> Context(TPlayer& actor)
    {
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = journal_.get(),
            .io_system = &io_,
        });
        context->actor = &actor;
        return context;
    }

    TAbilityOption Option(std::string_view name)
    {
        const auto options = battle_->AvailableActions(*actor_);
        const auto found = std::ranges::find(options, name, &TAbilityOption::DefinitionId);
        if (found == options.end()) {
            throw std::logic_error("missing test action");
        }
        return *found;
    }

    EAbilityExecutionResult Execute(const TAbilityOption& option,
                                     std::shared_ptr<TActionContext> context = {})
    {
        if (!context) {
            context = Context(*actor_);
        }
        return ExecuteAbility(option, std::move(context), *actor_,
            CollectAbilities(*actor_->GetCreature(), rules_->Catalog), rules_->Executions);
    }

    TGeneratedSourceBinding<TWeapon> Source(TPlayer& owner, const std::shared_ptr<TActionContext>& context)
    {
        return SourceCandidates<TCreature, TWeapon>(BindActionEntity<TCreature>(owner),
            TAbilitySourcePath{}.Field("weapons"),
            [](const TCreature& creature) -> const auto& { return creature.Weapons(); }).at(0).Snapshot(context);
    }

    std::optional<TNativeRecord::TInputs> Find() const
    {
        return FindActionRecord<TNativeRecord>(TGeneratedWorldView(battle_.get()), *actor_);
    }

    TAstNode Snapshot() const
    {
        TAstContext context;
        auto node = TAstNode::MakeObject("test");
        AddOwnedObject(node, "battle", *battle_, context);
        AddOwnedObject(node, "journal", *journal_, context);
        return node;
    }

    void ReplaceWeapons(TIdCollection<TWeapon> weapons)
    {
        auto execution = journal_->ExecutionContext();
        execution.AssignGuarded(actor_->GetCreature()->Weapons(), std::move(weapons),
                                 TGuarded<TCreature>(*actor_->GetCreature()), "weapons");
    }

    void TearDown() override { rng_.Verify(); }

    TMockRng rng_;
    TMockInteractionSystem io_;
    std::deque<TCreature> creatures_;
    std::shared_ptr<TAbilityRules> rules_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> journal_;
    TPlayer* actor_ = nullptr;
    TPlayer* target_ = nullptr;
    int amount_ = 3;
    int observed_amount_ = 0;
    TEntityId observed_target_;
    bool fail_body_ = false;
};

}

TEST_F(TActionHistoryTest, PublicationDetachesContextAndConsumerPreparesBeforeAcceptance)
{
    const auto initial = journal_->CurrentState();
    const auto before = Snapshot();
    auto context = Context(*actor_);
    std::weak_ptr<TActionContext> old_context = context;
    ASSERT_EQ(Execute(Option("history.Publish"), context), EAbilityExecutionResult::Applied);
    context.reset();
    EXPECT_TRUE(old_context.expired());
    ASSERT_TRUE(Find());
    EXPECT_EQ(Find()->Weapon.Read().BaseDiceSize(), 4);
    EXPECT_THROW(Find()->Target.Guard(), std::logic_error);
    ASSERT_EQ(Execute(Option("history.Consume")), EAbilityExecutionResult::Applied);
    EXPECT_EQ(observed_amount_, 3);
    EXPECT_EQ(observed_target_, target_->GetCreature()->Id());
    EXPECT_EQ(std::as_const(*target_->GetCreature()).Hitpoints().Current(), 13);
    EXPECT_FALSE(Find());
    EXPECT_EQ(battle_->History().Revision(*actor_), 2);
    journal_->Undo(initial);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TActionHistoryTest, StaleOptionRejectsBeforePaymentAndUndoRestoresItsExactStamp)
{
    ASSERT_EQ(Execute(Option("history.Publish")), EAbilityExecutionResult::Applied);
    const auto old_option = Option("history.Consume");
    const auto state = journal_->CurrentState();
    const auto before = Snapshot();
    amount_ = 5;
    ASSERT_EQ(Execute(Option("history.Publish")), EAbilityExecutionResult::Applied);
    const auto changed = Snapshot();
    EXPECT_EQ(Execute(old_option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(Snapshot(), changed);
    journal_->Undo(state);
    EXPECT_EQ(Snapshot(), before);
    ASSERT_EQ(Execute(old_option), EAbilityExecutionResult::Applied);
    EXPECT_EQ(observed_amount_, 3);
}

TEST_F(TActionHistoryTest, DivergentReplayCannotAliasEqualRevisionAndEqualSemanticAst)
{
    const auto initial = journal_->CurrentState();
    ASSERT_EQ(Execute(Option("history.Publish")), EAbilityExecutionResult::Applied);
    const auto old_option = Option("history.Consume");
    const auto first = Snapshot();
    journal_->Undo(initial);
    ASSERT_EQ(Execute(Option("history.Publish")), EAbilityExecutionResult::Applied);
    EXPECT_EQ(battle_->History().Revision(*actor_), 1);
    EXPECT_EQ(Snapshot(), first);
    EXPECT_EQ(Execute(old_option), EAbilityExecutionResult::Unavailable);
    EXPECT_EQ(Snapshot(), first);
    EXPECT_EQ(Execute(Option("history.Consume")), EAbilityExecutionResult::Applied);
}

TEST_F(TActionHistoryTest, DetachedSourceKeepsSnapshotWithoutOldGrantAndRestoresSparseIdentity)
{
    ASSERT_EQ(Execute(Option("history.Publish")), EAbilityExecutionResult::Applied);
    const auto stored = journal_->CurrentState();
    const auto before = Snapshot();
    ReplaceWeapons(Weapons(12, true, false));
    ASSERT_TRUE(Find());
    EXPECT_EQ(Find()->Weapon.Read().BaseDiceSize(), 4);
    EXPECT_EQ(Find()->Weapon.Key().Path, TAbilitySourcePath{}.Field("weapons").Entry(7));
    EXPECT_TRUE(battle_->AvailableActions(*actor_).empty());
    ReplaceWeapons(Weapons(12, false));
    EXPECT_FALSE(Find());
    journal_->Undo(stored);
    EXPECT_EQ(Snapshot(), before);
    ASSERT_TRUE(Find());
    EXPECT_EQ(Find()->Weapon.Read().BaseDiceSize(), 4);
}

TEST_F(TActionHistoryTest, ConsumedRecordSourceValidatesNestedActivityInputsWithoutRecursion)
{
    ASSERT_EQ(Execute(Option("history.Publish")), EAbilityExecutionResult::Applied);
    auto context = Context(*actor_);
    ASSERT_EQ(Execute(Option("history.Consume"), context), EAbilityExecutionResult::Applied);
    EXPECT_FALSE(Find());
    const auto input = PreparedActionRecord<TNativeRecord>(context);
    auto child = ForkActionInputs(context, {input.Weapon.InputValidator()});
    auto nested = ForkActionInputs(child, {input.Weapon.InputValidator()});
    EXPECT_NO_THROW(ValidateActionActor(nested));
    EXPECT_NO_THROW(input.Weapon.Validate(nested));
    EXPECT_EQ(input.Weapon.Read().BaseDiceSize(), 4);

    const auto before_removal = journal_->CurrentState();
    const auto snapshot = Snapshot();
    ReplaceWeapons(Weapons(12, false));
    EXPECT_THROW(ValidateActionActor(nested), TAbilityUnavailable);
    journal_->Undo(before_removal);
    EXPECT_EQ(Snapshot(), snapshot);
    EXPECT_NO_THROW(ValidateActionActor(nested));
    TBattle foreign(TBattleMap(3, 3), &rng_, io_, rules_);
    auto foreign_context = std::make_shared<TActionContext>(*nested);
    foreign_context->battle = &foreign;
    EXPECT_THROW(input.Weapon.Validate(foreign_context), TAbilityUnavailable);
    foreign_context->battle = nullptr;
    EXPECT_THROW(input.Weapon.Validate(foreign_context), TAbilityUnavailable);
    EXPECT_THROW(input.Weapon.Validate(nullptr), std::invalid_argument);
    EXPECT_EQ(Snapshot(), snapshot);
}

TEST_F(TActionHistoryTest, ForeignSourcePublicationRejectsBeforeHistoryWrite)
{
    auto context = Context(*actor_);
    AcceptActionExecution(context, *actor_, EActionHistoryKind::Action);
    const auto foreign = Source(*target_, context);
    const auto before = Snapshot();
    EXPECT_THROW(PublishActionRecord<TNativeRecord>(context, CaptureActionTicket(context), foreign,
        {BindActionEntity<TCreature>(*target_), 1}), TAbilityUnavailable);
    EXPECT_EQ(Snapshot(), before);
    EXPECT_FALSE(Find());
}

TEST_F(TActionHistoryTest, DetachedTargetRejectsForeignWorld)
{
    auto context = Context(*actor_);
    const auto recorded = TRecordedEntity<TCreature>::Capture(
        context, BindActionEntity<TCreature>(*target_));
    auto stranger = TCreature::Create({});
    TBattle other(TBattleMap(3, 3), &rng_, io_, rules_);
    rng_.ExpectCall(20, 10);
    other.AddPlayer(TPlayer(&stranger, TPlayerTeam{1}, TPlayerId{1}, "Stranger", ""), {0, 0});
    EXPECT_THROW(recorded.ReadOnly(TGeneratedWorldView(&other)), TAbilityUnavailable);
    EXPECT_EQ(recorded.ReadOnly(TGeneratedWorldView(battle_.get())).Read().Id(), target_->GetCreature()->Id());
    const auto before = Snapshot();
    auto foreign = other.GetIfPlayers([](const auto*) { return true; }).at(0);
    EXPECT_THROW(battle_->AcceptActionHistory(*foreign, EActionHistoryKind::Action, "foreign", *journal_),
                 TAbilityUnavailable);
    EXPECT_EQ(Snapshot(), before);
}

TEST_F(TActionHistoryTest, ReceiptPayloadAndTargetAreSemanticStateWithoutAutomaticTurnExpiry)
{
    const auto initial = journal_->CurrentState();
    ASSERT_EQ(Execute(Option("history.Publish")), EAbilityExecutionResult::Applied);
    const auto first = Snapshot();
    journal_->Undo(initial);
    amount_ = 9;
    ASSERT_EQ(Execute(Option("history.Publish")), EAbilityExecutionResult::Applied);
    EXPECT_NE(Snapshot(), first);
    auto execution = journal_->ExecutionContext();
    execution.Invoke(TGuarded<TCreature>(*actor_->GetCreature()), [&](TCreature& creature, TGeneratedExecutionContext&) {
        creature.EndTurn(execution);
        creature.BeginRound(execution);
        creature.BeginTurn(execution);
    });
    ASSERT_TRUE(Find());
    EXPECT_EQ(Find()->Amount, 9);
    EXPECT_EQ(battle_->History().Revision(*actor_), 1);
}

TEST_F(TActionHistoryTest, RegistryRejectsConflictsAndUnboundWorldHasNoPreviousRecord)
{
    EXPECT_NO_THROW(rules_->Records.Register<TNativeRecord>());
    EXPECT_THROW(rules_->Records.Register<TConflictingRecord>(), std::invalid_argument);
    EXPECT_THROW(rules_->Records.Require("missing", typeid(TNativeRecord)), std::invalid_argument);
    EXPECT_THROW(rules_->Records.Require(TNativeRecord::kId, typeid(TConflictingRecord)),
                 std::invalid_argument);
    EXPECT_FALSE(FindActionRecord<TNativeRecord>(TGeneratedWorldView{}, *actor_));
    auto context = Context(*actor_);
    context->battle = nullptr;
    EXPECT_NO_THROW(AcceptActionExecution(context, *actor_, EActionHistoryKind::Action));
    EXPECT_FALSE(CaptureActionTicket(context));
    EXPECT_EQ(battle_->History().Revision(*actor_), 0);
}

TEST(ActionHistoryAstTest, NormalizedLiveIdentitiesCompareEqualButStaleIdsAndTypesDoNot)
{
    auto first_creature = TCreature::Create({});
    auto second_creature = TCreature::Create({});
    TPlayer first(&first_creature, TPlayerTeam{0}, TPlayerId{0}, "Actor", "");
    TPlayer second(&second_creature, TPlayerTeam{0}, TPlayerId{0}, "Actor", "");
    ASSERT_NE(first_creature.Id(), second_creature.Id());
    TRecordedEntityKey key{first_creature.Id(), 0, &first, &first_creature, typeid(TCreature)};
    const TRecordedEntityKey independent{
        second_creature.Id(), 0, &second, &second_creature, typeid(TCreature),
    };
    TAstContext first_context;
    first_context.RegisterIdentity(&first, "actor");
    first_context.RegisterIdentity(&first_creature, "actor.creature");
    TAstContext second_context;
    second_context.RegisterIdentity(&second, "actor");
    second_context.RegisterIdentity(&second_creature, "actor.creature");
    const auto valid = key.GetAst(first_context);
    EXPECT_EQ(valid, independent.GetAst(second_context));
    key.Id = second_creature.Id();
    EXPECT_FALSE(key.Matches(first));
    EXPECT_NE(valid, key.GetAst(first_context));
    key.Id = first_creature.Id();
    key.Type = typeid(TWeapon);
    EXPECT_FALSE(key.Matches(first));
    EXPECT_NE(valid, key.GetAst(first_context));
}

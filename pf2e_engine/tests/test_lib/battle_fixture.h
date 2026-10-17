#pragma once

#include "mock_dice_roller.h"
#include "scripted_io.h"
#include "state_diff.h"

#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/rules.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <any>
#include <array>
#include <deque>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

inline TIdCollection<TFeatureSelection> Features(std::initializer_list<std::string> names)
{
    TIdCollection<TFeatureSelection> result;
    for (const auto& name : names) {
        result.Add(TFeatureSelection(name));
    }
    return result;
}

inline TStatisticBaseline Fixed(int value)
{
    return TStatisticBaseline::Create({.IsFixed = true, .Value = value});
}

struct TPick {
    TPick(const char* name) : TPick(-1, name) {}
    TPick(std::string name) : TPick(-1, std::move(name)) {}
    TPick(int actor, std::string name)
        : Actor(actor)
        , Match([name = std::move(name)](const TAlternative& alternative) { return alternative.name == name; })
    {
    }

    template <class T>
    static TPick Value(T value, int actor = -1)
    {
        TPick pick(actor, {});
        pick.Match = [value](const TAlternative& alternative) {
            const auto* actual = std::any_cast<T>(&alternative.value);
            return actual != nullptr && *actual == value;
        };
        return pick;
    }

    TPick WhenOffered(TScriptedIo::FObserve observe) const
    {
        auto result = *this;
        result.Observe = std::move(observe);
        return result;
    }

    int Actor;
    TScriptedIo::FMatch Match;
    TScriptedIo::FObserve Observe = {};
};

struct TStep {
    std::vector<TPick> Choose = {};
    std::vector<std::pair<int, int>> Rolls = {};
    std::map<std::string, TField> Changes = {};
    EAbilityExecutionResult Result = EAbilityExecutionResult::Applied;
};

class TBattleFixture : public ::testing::Test {
protected:
    void StartBattle(std::shared_ptr<const TAbilityRules> rules, int size, std::string prefix = {})
    {
        rules_ = std::move(rules);
        prefix_ = std::move(prefix);
        battle_ = std::make_unique<TBattle>(TBattleMap(size, size), &rng_, io_, rules_);
        journal_ = std::make_unique<TTransformator>(io_);
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
    }

    TPlayer& Join(TCreature creature, int team, TPosition position, std::string name = {}, int initiative = 10)
    {
        const int id = static_cast<int>(creatures_.size());
        if (name.empty()) {
            name = creature.Name();
        }
        creatures_.push_back(std::move(creature));
        rng_.ExpectCall(20, initiative);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{team}, TPlayerId{id}, std::move(name), ""), position);
        return Player(id);
    }

    TPlayer& Player(int id)
    {
        return *battle_->GetIfPlayers([id](const TPlayer* player) { return player->GetId() == id; }).at(0);
    }

    TCreature& Creature(int id) { return *Player(id).GetCreature(); }
    const TCreature& Read(int id) { return Creature(id); }

    std::shared_ptr<TActionContext> Context(int actor = 0)
    {
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = journal_.get(),
            .io_system = &io_,
        });
        const auto players = battle_->GetIfPlayers([actor](const TPlayer* player) { return player->GetId() == actor; });
        if (!players.empty()) {
            context->actor = players.front();
        }
        return context;
    }

    std::vector<TAbilityOption> Options(const std::string& name, int actor = 0)
    {
        const auto before = Snapshot();
        auto result = battle_->AvailableActions(Player(actor));
        ExpectSame(before, Snapshot(), "enumerating actions");
        std::erase_if(result, [&](const TAbilityOption& option) { return option.DefinitionId != prefix_ + name; });
        return result;
    }

    TAbilityOption Option(const std::string& name, int actor = 0, size_t index = 0)
    {
        const auto options = Options(name, actor);
        if (index >= options.size()) {
            throw std::logic_error("missing action option: " + name);
        }
        return options[index];
    }

    bool Available(const std::string& name, int actor = 0) { return !Options(name, actor).empty(); }

    EAbilityExecutionResult Execute(const TAbilityOption& option)
    {
        const int actor = option.ActorId;
        last_context_ = Context(actor);
        return ExecuteAbility(option, last_context_, Player(actor), CollectAbilities(Read(actor), rules_->Catalog),
            rules_->Executions,
            {.ValidateSource = [this, actor, definition = option.DefinitionId, source = option.Source] {
                return CollectAbilities(Read(actor), rules_->Catalog).FindGrant(definition, source) != nullptr;
            }});
    }

    EAbilityExecutionResult Execute(const std::string& name, int actor = 0, size_t index = 0)
    {
        return Execute(Option(name, actor, index));
    }

    void Act(const TAbilityOption& option, const TStep& step)
    {
        const auto before = Snapshot();
        const auto start = journal_->CurrentState();
        Queue(option, step, 0, 0);
        EXPECT_EQ(Execute(option), step.Result);
        ExpectDrained();
        const auto after = Snapshot();
        EXPECT_EQ(ChangedFields(Tracked(before), Tracked(after)), ExpectedFields(step.Changes));
        if (step.Result != EAbilityExecutionResult::Applied) {
            ExpectSame(before, after, "rejected action");
            return;
        }
        journal_->Undo(start);
        ExpectSame(before, Snapshot(), "undo");
        Replay(option, step, after);
    }

    void Act(const std::string& name, int actor, const TStep& step) { Act(Option(name, actor), step); }

    void Check(const TAbilityOption& option, const TStep& step)
    {
        const auto start = journal_->CurrentState();
        Act(option, step);
        journal_->Undo(start);
    }

    static TFields Tracked(const TAstNode& snapshot)
    {
        static constexpr std::string_view kPlayers = "players.";
        static constexpr std::array<std::string_view, 5> kIgnored = {"transformator", "action_history", "battle_map",
            "ability_rules", "relations"};
        TFields names;
        TFields fields;
        for (auto& [path, value] : AstFields(snapshot)) {
            if (!path.starts_with("battle.")) {
                continue;
            }
            auto field = path.substr(7);
            if (std::ranges::any_of(kIgnored, [&](std::string_view prefix) { return field.starts_with(prefix); })) {
                continue;
            }
            if (field.starts_with(kPlayers) && field.ends_with(".name") &&
                std::ranges::count(field.substr(kPlayers.size()), '.') == 1) {
                names.emplace(field.substr(kPlayers.size(), field.size() - kPlayers.size() - 5), value);
            }
            fields.emplace(std::move(field), std::move(value));
        }
        TFields result;
        for (auto& [path, value] : fields) {
            if (!path.starts_with(kPlayers)) {
                result.emplace(path, std::move(value));
                continue;
            }
            const auto rest = path.substr(kPlayers.size());
            const auto dot = rest.find('.');
            auto field = rest.substr(dot + 1);
            if (field.starts_with("creature.")) {
                field = field.substr(9);
            } else if (field != "position" && field != "team") {
                continue;
            }
            result.emplace(names.at(rest.substr(0, dot)) + "." + field, std::move(value));
        }
        return result;
    }

    static TSavepointStackUnwind Pause(const std::function<void()>& work)
    {
        try {
            work();
        } catch (const TSavepointStackUnwind& point) {
            return point;
        }
        throw std::logic_error("expected a suspension");
    }

    TAstNode Snapshot() const
    {
        TAstContext context;
        auto result = TAstNode::MakeObject("battle_fixture");
        AddOwnedObject(result, "battle", *battle_, context);
        AddOwnedObject(result, "journal", *journal_, context);
        return result;
    }

    void Queue(const TAbilityOption& option, const TStep& step, size_t rolls, size_t choices)
    {
        for (size_t index = rolls; index < step.Rolls.size(); ++index) {
            rng_.ExpectCall(step.Rolls[index].first, step.Rolls[index].second);
        }
        for (size_t index = choices; index < step.Choose.size(); ++index) {
            const auto& pick = step.Choose[index];
            io_.Expect(pick.Actor < 0 ? option.ActorId : pick.Actor, {}, pick.Match, false, pick.Observe);
        }
    }

    void ExpectDrained()
    {
        EXPECT_EQ(rng_.RemainingCalls(), 0u);
        EXPECT_EQ(io_.Pending(), 0u);
    }

    void Replay(const TAbilityOption& option, const TStep& step, const TAstNode& after)
    {
        struct TPoint {
            TSavepointStackUnwind Point;
            TAstNode State;
            size_t Rolls;
            size_t Choices;
        };
        std::vector<TPoint> points;
        io_.SuspendRequests(true);
        Queue(option, step, 0, 0);
        std::function<void()> next = [&] { Execute(option); };
        for (;;) {
            try {
                next();
                break;
            } catch (const TSavepointStackUnwind& point) {
                points.push_back({point, Snapshot(), step.Rolls.size() - rng_.RemainingCalls(),
                                  step.Choose.size() - io_.Pending()});
                next = [point] { point.Resume(); };
            }
        }
        io_.SuspendRequests(false);
        ExpectSame(after, Snapshot(), "resumed action");
        for (auto point = points.rbegin(); point != points.rend(); ++point) {
            point->Point.Revert(*journal_);
            ExpectSame(point->State, Snapshot(), "reverted savepoint");
            Queue(option, step, point->Rolls, point->Choices);
            point->Point.Resume();
            ExpectDrained();
            ExpectSame(after, Snapshot(), "replayed savepoint");
        }
    }

    std::shared_ptr<const TAbilityRules> rules_;
    std::string prefix_;
    TScriptedIo io_;
    TMockRng rng_;
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> journal_;
    std::shared_ptr<TActionContext> last_context_;
};

class TGeneratedCombatFixture : public TBattleFixture {
protected:
    void SetUp() override { StartBattle(CreateAbilityRules(), 20); }

    void BeginTurn(int actor = 0)
    {
        auto execution = journal_->ExecutionContext();
        execution.Invoke(TGuarded<TCreature>(Creature(actor)), &TCreature::BeginTurn);
    }
};

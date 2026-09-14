#pragma once

#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

#include "mock_dice_roller.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <any>
#include <deque>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

class TGeneratedCombatIo final : public IInteractionSystem {
public:
    void Expect(int actor, std::string kind,
                std::function<bool(const TAlternative&)> matches,
                bool suspend = false, std::function<void()> observe = {})
    {
        expected_.push_back({actor, std::move(kind), std::move(matches), suspend, std::move(observe)});
    }

    void ExpectName(int actor, std::string kind, std::string name, bool suspend = false)
    {
        Expect(actor, std::move(kind), [name = std::move(name)](const auto& alternative) {
            return alternative.name == name;
        }, suspend);
    }

    template <class T>
    void ExpectValue(int actor, T value, bool suspend = false)
    {
        Expect(actor, {}, [value](const auto& alternative) {
            const auto* actual = std::any_cast<T>(&alternative.value);
            return actual != nullptr && *actual == value;
        }, suspend);
    }

    void RequestChoice(int actor, TAlternatives alternatives, TState state,
                       std::function<void(size_t)> receive) override
    {
        if (expected_.empty()) {
            throw std::logic_error("unexpected generated combat choice: " + std::string(alternatives.Kind()));
        }
        auto expected = std::move(expected_.front());
        expected_.pop_front();
        EXPECT_EQ(actor, expected.Actor);
        if (!expected.Kind.empty()) {
            EXPECT_EQ(alternatives.Kind(), expected.Kind);
        }
        size_t index = 0;
        while (index < alternatives.Size() && !expected.Matches(alternatives[index])) {
            ++index;
        }
        if (index == alternatives.Size()) {
            throw std::logic_error("missing generated combat alternative");
        }
        if (expected.Observe) {
            expected.Observe();
        }
        ++requests_;
        if (expected.Suspend) {
            throw TSavepointStackUnwind(state, [receive = std::move(receive), index]() { receive(index); });
        }
        receive(index);
    }

    void HandleReactionTrigger(const TTriggerContext&, const TState&) override
    {
        throw std::logic_error("unexpected legacy reaction trigger");
    }
    std::ostream& GameLog() override { return log_; }
    std::ostream& DevLog() override { return log_; }
    size_t Requests() const { return requests_; }
    void Verify() const { EXPECT_TRUE(expected_.empty()); }

protected:
    size_t ChooseAlternativeIndex(int, const TAlternatives&) override
    {
        throw std::logic_error("unexpected synchronous choice");
    }

private:
    struct TExpected {
        int Actor;
        std::string Kind;
        std::function<bool(const TAlternative&)> Matches;
        bool Suspend;
        std::function<void()> Observe;
    };
    std::deque<TExpected> expected_;
    std::ostringstream log_;
    size_t requests_ = 0;
};

class TGeneratedCombatFixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        rules_ = factory_.AbilityRules();
        nlohmann::json map = {{"x_size", 20}, {"y_size", 20}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, rules_);
        journal_ = std::make_unique<TTransformator>(io_);
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
    }

    TPlayer& Add(TCreature creature, int team, TPosition position, int initiative = 10)
    {
        const int id = static_cast<int>(creatures_.size());
        const std::string name = creature.Name();
        creatures_.push_back(std::move(creature));
        rng_.ExpectCall(20, initiative);
        battle_->AddPlayer(TPlayer(&creatures_.back(), TPlayerTeam{team}, TPlayerId{id}, name, ""), position);
        return Player(id);
    }

    TPlayer& Player(int id)
    {
        return *battle_->GetIfPlayers([id](const auto* player) { return player->GetId() == id; }).at(0);
    }

    TCreature& Creature(int id) { return *Player(id).GetCreature(); }
    const TCreature& Read(int id) { return Creature(id); }

    std::shared_ptr<TActionContext> Context(int actor = 0)
    {
        auto context = std::make_shared<TActionContext>(TActionContext{
            .battle = battle_.get(), .dice_roller = &rng_, .transformator = journal_.get(),
            .io_system = &io_, .scheduler = nullptr, .effect_manager = nullptr,
        });
        context->game_object_registry = std::make_shared<TGameObjectRegistry>();
        context->game_object_registry->Add(TGameObjectIdManager::Instance().Register("self"), &Player(actor));
        return context;
    }

    bool Available(const std::string& name, int actor = 0)
    {
        return std::ranges::any_of(battle_->AvailableActions(Player(actor)),
            [&](const auto& option) { return option.DefinitionId == name; });
    }

    EAbilityExecutionResult Execute(const std::string& name, int actor = 0)
    {
        const auto options = battle_->AvailableActions(Player(actor));
        const auto found = std::ranges::find(options, name, &TAbilityOption::DefinitionId);
        if (found == options.end()) {
            throw std::logic_error("missing generated combat action: " + name);
        }
        return ExecuteAbility(*found, Context(actor), Player(actor),
            CollectAbilities(Read(actor), rules_->Catalog), rules_->Executions);
    }

    void BeginTurn(int actor = 0)
    {
        auto execution = journal_->ExecutionContext();
        execution.Invoke(TGuarded<TCreature>(Creature(actor)), &TCreature::BeginTurn);
    }

    static TSavepointStackUnwind Pause(const std::function<void()>& work)
    {
        try {
            work();
        } catch (const TSavepointStackUnwind& point) {
            return point;
        }
        throw std::logic_error("expected a generated combat suspension");
    }

    TAstNode Snapshot()
    {
        TAstContext context;
        auto result = TAstNode::MakeObject("generated_combat_test");
        AddOwnedObject(result, "battle", *battle_, context);
        AddOwnedObject(result, "journal", *journal_, context);
        return result;
    }

    TGameObjectFactory factory_;
    std::shared_ptr<const TAbilityRules> rules_;
    TGeneratedCombatIo io_;
    TMockRng rng_;
    std::deque<TCreature> creatures_;
    std::unique_ptr<TBattle> battle_;
    std::unique_ptr<TTransformator> journal_;
};

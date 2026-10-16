#pragma once

#include "mock_dice_roller.h"
#include "scripted_io.h"

#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/ast/ast_helpers.h>
#include <pf2e_engine/game_object_logic/game_object_factory.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <deque>
#include <functional>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class TBattleFixture : public ::testing::Test {
protected:
    void StartBattle(std::shared_ptr<const TAbilityRules> rules, int size, std::string prefix = {})
    {
        rules_ = std::move(rules);
        prefix_ = std::move(prefix);
        nlohmann::json map = {{"x_size", size}, {"y_size", size}};
        battle_ = std::make_unique<TBattle>(TBattleMap(map), &rng_, io_, rules_);
        journal_ = std::make_unique<TTransformator>(io_);
    }

    void TearDown() override
    {
        rng_.Verify();
        io_.Verify();
        EXPECT_EQ(io_.LegacyCalls(), 0);
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
            .io_system = &io_, .scheduler = nullptr, .effect_manager = nullptr,
        });
        context->game_object_registry = std::make_shared<TGameObjectRegistry>();
        const auto players = battle_->GetIfPlayers([actor](const TPlayer* player) { return player->GetId() == actor; });
        if (!players.empty()) {
            context->game_object_registry->Add(TGameObjectIdManager::Instance().Register("self"), players.front());
        }
        return context;
    }

    std::vector<TAbilityOption> Options(const std::string& name, int actor = 0)
    {
        auto result = battle_->AvailableActions(Player(actor));
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
                const auto fresh = CollectAbilities(Read(actor), rules_->Catalog);
                return std::ranges::any_of(fresh.Grants(), [&](const TGrantedAbility& grant) {
                    return grant.DefinitionId == definition && grant.Source == source;
                });
            }});
    }

    EAbilityExecutionResult Execute(const std::string& name, int actor = 0, size_t index = 0)
    {
        return Execute(Option(name, actor, index));
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

    static TIdCollection<TFeatureSelection> Features(std::initializer_list<std::string> names)
    {
        TIdCollection<TFeatureSelection> result;
        for (const auto& name : names) {
            result.Add(TFeatureSelection(name));
        }
        return result;
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
    void SetUp() override { StartBattle(TGameObjectFactory().AbilityRules(), 20); }

    void BeginTurn(int actor = 0)
    {
        auto execution = journal_->ExecutionContext();
        execution.Invoke(TGuarded<TCreature>(Creature(actor)), &TCreature::BeginTurn);
    }
};

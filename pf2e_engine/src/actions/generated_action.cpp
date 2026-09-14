#include <pf2e_engine/actions/generated_action.h>

#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/rule_integer.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/random.h>

#include <limits>
#include <string>

namespace {

const TGameObjectId kSelfId = TGameObjectIdManager::Instance().Register("self");

void RequireContext(const std::shared_ptr<TActionContext>& ctx)
{
    if (!ctx || ctx->transformator == nullptr) {
        throw std::invalid_argument("generated action requires an execution context");
    }
    ValidateAbilitySource(ctx);
}

}

void TGeneratedEntityView::Validate() const
{
    if (player_ == nullptr || address_ == nullptr || id_.Value == 0) {
        throw TAbilityUnavailable();
    }
    if (battle_ != nullptr &&
        battle_->GetPlayer([this](const TPlayer* current) { return current == player_; }) == nullptr) {
        throw TAbilityUnavailable();
    }
    const auto* creature = player_->GetCreature();
    if (creature != address_ || type_ != typeid(TCreature) || creature->Id() != id_) {
        throw TAbilityUnavailable();
    }
}

const TPlayer& TGeneratedEntityView::Player() const
{
    Validate();
    return *player_;
}

TPlayer& TGeneratedEntityView::MutablePlayer() const
{
    Validate();
    if (mutable_player_ == nullptr) {
        throw std::logic_error("entity binding has no mutable participant");
    }
    return *mutable_player_;
}

TGeneratedEntityView TGeneratedEntityView::ReadOnly() const
{
    Validate();
    auto result = *this;
    result.mutable_address_ = nullptr;
    result.mutable_player_ = nullptr;
    return result;
}

TGeneratedEntityView BindGeneratedActionEntity(const TPlayer& player, const TBattle* battle)
{
    const auto* creature = player.GetCreature();
    if (creature == nullptr) {
        throw TAbilityUnavailable();
    }
    TGeneratedEntityView result;
    result.type_ = typeid(TCreature);
    result.id_ = creature->Id();
    result.address_ = creature;
    result.player_ = &player;
    result.battle_ = battle;
    result.Validate();
    return result;
}

TGeneratedEntityView BindGeneratedActionEntity(TPlayer& player, const TBattle* battle)
{
    auto result = BindGeneratedActionEntity(std::as_const(player), battle);
    result.mutable_address_ = player.GetCreature();
    result.mutable_player_ = &player;
    return result;
}

TGeneratedEntityView GeneratedActionSelf(const std::shared_ptr<TActionContext>& ctx)
{
    RequireContext(ctx);
    if (!ctx->game_object_registry) {
        throw std::invalid_argument("generated action requires an actor binding");
    }
    auto view = BindGeneratedActionEntity(ctx->game_object_registry->Get<TPlayer>(kSelfId), ctx->battle);
    if (!view.Player().GetCreature()->IsAlive()) {
        throw TAbilityUnavailable();
    }
    return view;
}

void ValidateActionActor(const std::shared_ptr<TActionContext>& ctx)
{
    GeneratedActionSelf(ctx).Validate();
}

void ValidateActionEntity(const std::shared_ptr<TActionContext>& ctx,
                          const TGeneratedEntityView& entity)
{
    RequireContext(ctx);
    const auto* player = &entity.Player();
    if (ctx->battle == nullptr ||
        ctx->battle->GetPlayer([player](const TPlayer* current) { return current == player; }) == nullptr) {
        throw TAbilityUnavailable();
    }
}

const TAbilityRules& GeneratedActionRules(const std::shared_ptr<TActionContext>& ctx)
{
    ValidateActionActor(ctx);
    if (ctx->battle == nullptr) {
        throw std::invalid_argument("generated action rules require a battle");
    }
    return ctx->battle->AbilityRules();
}

std::vector<TGeneratedEntityView> GeneratedActionEntities(const std::shared_ptr<TActionContext>& ctx,
                                                        std::type_index type)
{
    ValidateActionActor(ctx);
    if (ctx->battle == nullptr) {
        throw std::invalid_argument("entity enumeration requires a battle");
    }
    std::vector<TGeneratedEntityView> result;
    if (type != typeid(TCreature)) {
        return result;
    }
    for (auto* player : ctx->battle->GetIfPlayers([](const TPlayer*) { return true; })) {
        result.push_back(BindGeneratedActionEntity(*player, ctx->battle));
    }
    return result;
}

void RequestActionEntity(const std::shared_ptr<TActionContext>& ctx,
                         std::vector<TGeneratedEntityView> candidates,
                         std::function<void(TGeneratedEntityView)> receive)
{
    RequireContext(ctx);
    if (ctx->io_system == nullptr || !receive) {
        throw std::invalid_argument("entity choice requires interaction and a receiver");
    }
    if (candidates.empty()) {
        throw TAbilityUnavailable();
    }
    const auto actor = GeneratedActionSelf(ctx);
    auto alternatives = TAlternatives::Create<TPlayer*>("target");
    for (const auto& candidate : candidates) {
        ValidateActionEntity(ctx, candidate);
        const auto& player = candidate.Player();
        alternatives.AddAlternative(std::string(player.GetName()), &candidate.MutablePlayer());
    }
    const auto received = std::make_shared<bool>(false);
    auto choose = [ctx, actor, candidates = std::move(candidates), receive = std::move(receive),
                   received](size_t index) {
        ValidateActionActor(ctx);
        actor.Validate();
        if (index >= candidates.size()) {
            throw std::out_of_range("selected entity is outside the alternatives");
        }
        ValidateActionEntity(ctx, candidates[index]);
        *received = true;
        receive(candidates[index]);
    };
    ctx->io_system->RequestChoice(actor.Player().GetId(), std::move(alternatives),
                                  ctx->transformator->CurrentState(), std::move(choose));
    if (!*received) {
        throw std::logic_error("choice request must complete or suspend through a savepoint");
    }
}

int RollActionDice(const std::shared_ptr<TActionContext>& ctx, int count, int size)
{
    ValidateActionActor(ctx);
    if (ctx->dice_roller == nullptr || count < 0 || size <= 0) {
        throw std::invalid_argument("dice require a nonnegative count and positive size");
    }
    if (count != 0 && size > std::numeric_limits<int>::max() / count) {
        throw std::overflow_error("dice total can exceed the rule integer range");
    }
    int total = 0;
    for (int i = 0; i < count; ++i) {
        const int roll = ctx->dice_roller->RollDice(size);
        if (roll < 1 || roll > size) {
            throw std::out_of_range("random generator returned a value outside the die range");
        }
        total = TRuleInteger::Add(total, roll);
    }
    return total;
}

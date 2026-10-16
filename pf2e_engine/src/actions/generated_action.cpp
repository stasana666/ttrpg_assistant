#include <pf2e_engine/actions/generated_action.h>

#include <pf2e_engine/battle.h>
#include <pf2e_engine/actions/save_point.h>
#include <pf2e_engine/common/rule_integer.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/i_interaction_system.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/random.h>

#include <limits>
#include <optional>
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
    if (ctx->execution_mode == EActionExecutionMode::System) {
        if (ctx->battle == nullptr) {
            throw std::invalid_argument("system execution requires battle membership");
        }
    } else if (ctx->execution_mode != EActionExecutionMode::Ordinary ||
               !view.Player().GetCreature()->CanAct()) {
        throw TAbilityUnavailable();
    }
    return view;
}

void ValidateActionActor(const std::shared_ptr<TActionContext>& ctx)
{
    GeneratedActionSelf(ctx).Validate();
}

void ValidateActionActorBinding(const std::shared_ptr<TActionContext>& ctx,
                                const TGeneratedEntityView& actor)
{
    actor.Validate();
    const auto current = GeneratedActionSelf(ctx);
    if (current != actor || &current.Player() != &actor.Player()) {
        throw TAbilityUnavailable();
    }
}

void ValidateActionEntity(const std::shared_ptr<TActionContext>& ctx,
                          const TGeneratedEntityView& entity)
{
    RequireContext(ctx);
    ValidateActionEntityMembership(ctx, entity);
}

void ValidateActionEntityMembership(const std::shared_ptr<TActionContext>& ctx,
                                    const TGeneratedEntityView& entity)
{
    if (!ctx || ctx->transformator == nullptr) {
        throw std::invalid_argument("entity membership requires an execution context");
    }
    const auto* player = entity.player_;
    if (ctx->battle == nullptr ||
        ctx->battle->GetPlayer([player](const TPlayer* current) { return current == player; }) == nullptr) {
        throw TAbilityUnavailable();
    }
    entity.Validate();
}

void ValidateActionInputs(const std::shared_ptr<TActionContext>& ctx)
{
    if (!ctx) {
        throw std::invalid_argument("action inputs require an execution context");
    }
    if (ctx->input_validators) {
        for (const auto& validate : *ctx->input_validators) {
            if (!validate) {
                throw std::invalid_argument("action input validator must not be empty");
            }
            validate(ctx);
        }
    }
}

std::shared_ptr<TActionContext> ForkActionInputs(
    const std::shared_ptr<TActionContext>& ctx, std::vector<FActionInputValidator> validators)
{
    ValidateActionActor(ctx);
    auto inputs = ctx->input_validators ? *ctx->input_validators
                                       : std::vector<FActionInputValidator>{};
    for (auto& validator : validators) {
        if (!validator) {
            throw std::invalid_argument("action input validator must not be empty");
        }
        inputs.push_back(std::move(validator));
    }
    auto child = std::make_shared<TActionContext>(*ctx);
    child->input_validators =
        std::make_shared<const std::vector<FActionInputValidator>>(std::move(inputs));
    ValidateActionActor(child);
    return child;
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

std::size_t SelectActionIndex(const std::shared_ptr<TActionContext>& ctx,
                              TAlternatives alternatives,
                              std::function<void(std::size_t)> on_resume)
{
    ValidateActionActor(ctx);
    if (ctx->io_system == nullptr || !on_resume) {
        throw std::invalid_argument("choice requires interaction and a continuation");
    }
    if (alternatives.Empty()) {
        throw TAbilityUnavailable();
    }
    enum class ERequestMode { Requesting, Suspended, Completed, Failed };
    struct TRequest {
        ERequestMode Mode = ERequestMode::Requesting;
        std::optional<std::size_t> Selected;
    };
    const auto actor = GeneratedActionSelf(ctx);
    auto validate_actor = [ctx, actor] {
        ValidateActionActorBinding(ctx, actor);
    };
    const auto request = std::make_shared<TRequest>();
    const auto size = alternatives.Size();
    auto receive = [validate_actor, request, size, on_resume = std::move(on_resume)](std::size_t index) {
        validate_actor();
        if (index >= size) {
            throw std::out_of_range("selected index is outside the alternatives");
        }
        if (request->Mode == ERequestMode::Requesting) {
            if (request->Selected) {
                throw std::logic_error("choice completed more than once before returning");
            }
            request->Selected = index;
            return;
        }
        if (request->Mode != ERequestMode::Suspended) {
            throw std::logic_error("only a suspended choice can be resumed");
        }
        on_resume(index);
    };
    try {
        ctx->io_system->RequestChoice(actor.Player().GetId(),
            std::move(alternatives), ctx->transformator->CurrentState(), std::move(receive));
    } catch (const TSavepointStackUnwind&) {
        request->Mode = ERequestMode::Suspended;
        throw;
    } catch (...) {
        request->Mode = ERequestMode::Failed;
        throw;
    }
    request->Mode = ERequestMode::Failed;
    validate_actor();
    if (!request->Selected) {
        throw std::logic_error("choice request must complete or suspend through a savepoint");
    }
    request->Mode = ERequestMode::Completed;
    return *request->Selected;
}

TGeneratedEntityView MutableSelectedActionEntity(const std::shared_ptr<TActionContext>& ctx,
                                                 const TGeneratedEntityView& selected)
{
    ValidateActionEntity(ctx, selected);
    const auto* identity = &selected.Player();
    for (auto* player : ctx->battle->GetIfPlayers([identity](const TPlayer* candidate) {
             return candidate == identity;
         })) {
        return BindGeneratedActionEntity(*player, ctx->battle);
    }
    throw TAbilityUnavailable();
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

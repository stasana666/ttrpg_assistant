#include <action_block.h>

#include <pf2e_engine/actions/action.h>
#include <pf2e_engine/actions/ability.h>
#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/battle.h>
#include <pf2e_engine/common/errors.h>
#include <pf2e_engine/common/continuation.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/feat.h>
#include <pf2e_engine/game_object_logic/game_object_registry.h>
#include <pf2e_engine/game_object_logic/game_object_id.h>
#include <pf2e_engine/game_object_logic/game_object.h>
#include <pf2e_engine/player.h>
#include <pf2e_engine/success_level.h>
#include <pf2e_engine/common/visit.h>

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <sstream>

const TGameObjectId kSuccessLevelId = TGameObjectIdManager::Instance().Register("value");
static const TGameObjectId kSelfId = TGameObjectIdManager::Instance().Register("self");

namespace {

struct TBoundFeatureHook {
    TGrantedAbility Grant;
    std::shared_ptr<const TCreatureFeat> Definition;
};

void RunWithFeatureValidation(const std::shared_ptr<TActionContext>& ctx,
                              const std::function<bool()>& validate,
                              const TSavepointCallback& callback)
{
    const auto previous = ctx->validate_ability;
    ctx->validate_ability = validate;
    try {
        ValidateAbilitySource(ctx);
        callback();
    } catch (TSavepointStackUnwind& save_point) {
        ctx->validate_ability = previous;
        save_point.AddCallFunctionLevel([ctx, validate](TSavepointCallback resume) {
            RunWithFeatureValidation(ctx, validate, resume);
        });
        throw;
    } catch (...) {
        ctx->validate_ability = previous;
        throw;
    }
    ctx->validate_ability = previous;
}

void RunFeatureHook(std::shared_ptr<TActionContext> ctx, const TBoundFeatureHook& hook)
{
    TPlayer* self = &ctx->game_object_registry->Get<TPlayer>(kSelfId);
    const auto validate = [previous = ctx->validate_ability, self, battle = ctx->battle,
                           definition = hook.Grant.DefinitionId, source = hook.Grant.Source]() {
        if (previous && !previous()) {
            return false;
        }
        const auto abilities = CollectAbilities(*self->GetCreature(), battle->AbilityRules().Catalog);
        return std::ranges::any_of(abilities.Features(), [&](const TGrantedAbility& feature) {
            return feature.DefinitionId == definition && feature.Source == source;
        });
    };
    RunWithFeatureValidation(ctx, validate, [ctx, definition = hook.Definition]() {
        RunSubPipeline(ctx, definition->pipeline.front().get());
    });
}

}

void IActionBlock::Run(std::shared_ptr<TActionContext> ctx)
{
    ValidateAbilitySource(ctx);
    ValidateActionActor(ctx);
    if (ctx->battle == nullptr) {
        throw std::invalid_argument("action feature hooks require a battle context");
    }
    TPlayer& self = ctx->game_object_registry->Get<TPlayer>(kSelfId);
    std::string name = GetName();

    const auto& rules = ctx->battle->AbilityRules();
    const auto abilities = CollectAbilities(*self.GetCreature(), rules.Catalog);
    std::vector<TBoundFeatureHook> feat_entries;
    for (const auto& feature : abilities.Features()) {
        const auto found = rules.FeatureHooks.find(feature.DefinitionId);
        if (found == rules.FeatureHooks.end()) {
            continue;
        }
        const auto& feat = found->second;
        if (!feat) {
            throw std::invalid_argument("null legacy feature hook definition");
        }
        if (feat->pipeline.empty()) {
            continue;
        }
        bool matches = std::find(feat->blocks.begin(), feat->blocks.end(), name)
            != feat->blocks.end();
        if (matches) {
            feat_entries.push_back(TBoundFeatureHook{feature, feat});
        }
    }

    continuation::Then(
        [ctx, feat_entries]() {
            continuation::ForEachOwned(feat_entries, [ctx](const TBoundFeatureHook& hook) {
                RunFeatureHook(ctx, hook);
            });
        },
        [this, ctx]() {
            ValidateAbilitySource(ctx);
            ValidateActionActor(ctx);
            Apply(ctx);
        });
}

constexpr std::string ToString(EBlockType type)
{
    switch (type) {
        case EBlockType::Switch:
            return "switch";
        case EBlockType::FunctionCall:
            return "function_call";
        case EBlockType::Terminate:
            return "terminate";
        case EBlockType::ForEach:
            return "for_each";
    }
    throw std::logic_error("unexpected BlockType");
}

EBlockType BlockTypeFromString(const std::string& type)
{
    for (auto t : {EBlockType::FunctionCall, EBlockType::Switch, EBlockType::Terminate, EBlockType::ForEach}) {
        if (type == ToString(t)) {
            return t;
        }
    }
    std::stringstream ss;
    ss << "unknown block type " << type << "\n";
    throw std::runtime_error(ss.str());
}

void TFunctionCallBlock::Apply(std::shared_ptr<TActionContext> ctx)
{
    continuation::Then(
        [this, ctx]() { apply_(ctx); },
        [this, ctx]() { ctx->next_block = next_; });
}

void TSwitchBlock::Apply(std::shared_ptr<TActionContext> ctx)
{
    std::visit(VisitorHelper{
        [&](ESuccessLevel success_level) {
            ctx->next_block = next_table_.at(success_level);
        },
        [](auto&&) {
            throw std::logic_error("unexpected type");
        }
    }, input_.Get(kSuccessLevelId, ctx));
}

void TTerminateBlock::Apply(std::shared_ptr<TActionContext> ctx)
{
    ctx->next_block = nullptr;
}

static const TGameObjectId kListId = TGameObjectIdManager::Instance().Register("list");

void TForEachBlock::Apply(std::shared_ptr<TActionContext> ctx)
{
    TPlayerList targets;
    std::visit(VisitorHelper{
        [&](TPlayerList list) {
            targets = std::move(list);
        },
        [](auto&&) {
            throw std::logic_error("unexpected type for list: TForEachBlock");
        }
    }, input_.Get(kListId, ctx));

    continuation::Then(
        [this, ctx, targets]() {
            continuation::ForEachOwned(targets, [this, ctx](TPlayer* target) {
                ctx->game_object_registry->Add(*element_id_, target);
                continuation::ForEach(body_.begin(), body_.end(),
                    [ctx](const std::unique_ptr<IActionBlock>& block) {
                        block->Run(ctx);
                    });
            });
        },
        [this, ctx]() { ctx->next_block = next_; });
}

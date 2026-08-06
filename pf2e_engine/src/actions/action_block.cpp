#include <action_block.h>

#include <pf2e_engine/actions/action.h>
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

void IActionBlock::Run(std::shared_ptr<TActionContext> ctx)
{
    TPlayer& self = ctx->game_object_registry->Get<TPlayer>(kSelfId);
    std::string name = GetName();

    std::vector<IActionBlock*> feat_entries;
    for (const auto& feat : self.GetCreature()->Feats()) {
        if (feat->pipeline.empty()) {
            continue;
        }
        bool matches = std::find(feat->blocks.begin(), feat->blocks.end(), name)
            != feat->blocks.end();
        if (matches) {
            feat_entries.push_back(feat->pipeline.begin()->get());
        }
    }

    continuation::Then(
        [ctx, feat_entries]() {
            continuation::ForEachOwned(feat_entries, [ctx](IActionBlock* first) {
                RunSubPipeline(ctx, first);
            });
        },
        [this, ctx]() { Apply(ctx); });
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
            ctx->next_block = next_table_[success_level];
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

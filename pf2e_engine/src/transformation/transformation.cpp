#include <pf2e_engine/transformation/transformation.h>
#include <pf2e_engine/creature.h>
#include <pf2e_engine/effect_manager.h>
#include <pf2e_engine/initiative_order.h>

#include <pf2e_engine/common/ast/ast_helpers.h>

TChangeHitPoints::TChangeHitPoints(THitPoints* hitpoints, int value)
    : hitpoints_(hitpoints)
    , prev_(*hitpoints_)
{
    if (value < 0) {
        hitpoints_->ReduceHp(-value);
    } else {
        hitpoints_->RestoreHp(value);
    }
}

void TChangeHitPoints::Undo()
{
    *hitpoints_ = prev_;
}

TChangeCondition::TChangeCondition(TCreature* creature, ECondition condition, int new_value)
    : creature_(creature)
    , condition_(condition)
    , prev_value_(creature->Get(condition))
{
    creature_->Set(condition_, new_value);
}

void TChangeCondition::Undo()
{
    creature_->Set(condition_, prev_value_);
}

TChangeResource::TChangeResource(TResourcePool* pool, TResourceId id, int delta)
    : pool_(pool)
    , id_(id)
    , delta_(delta)
{
    if (delta_ > 0) {
        pool_->Add(id_, delta_);
    } else if (delta_ < 0) {
        pool_->Reduce(id_, -delta_);
    }
}

void TChangeResource::Undo()
{
    if (delta_ > 0) {
        pool_->Reduce(id_, delta_);
    } else if (delta_ < 0) {
        pool_->Add(id_, -delta_);
    }
}

TAddEffect::TAddEffect(TEffectManager* manager, TPlayer* player, ECondition condition, int value)
    : manager_(manager)
    , player_(player)
    , condition_(condition)
    , value_(value)
{
    manager_->InsertValue(player_, condition_, value_);
}

void TAddEffect::Undo()
{
    manager_->EraseValue(player_, condition_, value_);
}

TRemoveEffect::TRemoveEffect(TEffectManager* manager, TPlayer* player, ECondition condition, int value)
    : manager_(manager)
    , player_(player)
    , condition_(condition)
    , value_(value)
{
    manager_->EraseValue(player_, condition_, value_);
}

void TRemoveEffect::Undo()
{
    manager_->InsertValue(player_, condition_, value_);
}

TAddTask::TAddTask(TTaskScheduler* scheduler, TTask task)
    : scheduler_(scheduler)
    , task_id_(scheduler_->AddTaskWithId(std::move(task)))
{
}

void TAddTask::Undo()
{
    scheduler_->RemoveTaskById(task_id_);
}

TRemoveTask::TRemoveTask(TTaskScheduler* scheduler, TTaskId id, TTask task, size_t progress_index)
    : scheduler_(scheduler)
    , task_id_(id)
    , task_(std::move(task))
    , progress_index_(progress_index)
{
    scheduler_->RemoveTaskById(task_id_);
}

void TRemoveTask::Undo()
{
    scheduler_->RestoreTask(task_id_, std::move(task_), progress_index_);
}

TAdvanceTaskProgress::TAdvanceTaskProgress(TTaskScheduler* scheduler, TTaskId id, size_t new_index)
    : scheduler_(scheduler)
    , task_id_(id)
    , prev_index_(scheduler_->GetTaskProgress(id))
{
    scheduler_->SetTaskProgress(task_id_, new_index);
}

void TAdvanceTaskProgress::Undo()
{
    scheduler_->SetTaskProgress(task_id_, prev_index_);
}

TChangeCurrentPlayer::TChangeCurrentPlayer(TInitiativeOrder* order, size_t new_position)
    : order_(order)
    , prev_position_(order_->GetCurrentPosition())
{
    order_->SetCurrentPosition(new_position);
}

void TChangeCurrentPlayer::Undo()
{
    order_->SetCurrentPosition(prev_position_);
}

TChangeRound::TChangeRound(TInitiativeOrder* order, size_t new_round)
    : order_(order)
    , prev_round_(order_->CurrentRound())
{
    order_->SetRound(new_round);
}

void TChangeRound::Undo()
{
    order_->SetRound(prev_round_);
}

TAstNode TChangeHitPoints::GetAst(TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TChangeHitPoints");
    AddReference(node, "hitpoints_ref", hitpoints_, ctx);
    AddOwnedObject(node, "prev", prev_, ctx);
    return node;
}

TAstNode TChangeCondition::GetAst(TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TChangeCondition");
    AddReference(node, "creature_ref", creature_, ctx);
    AddValueField(node, "condition", condition_);
    AddValueField(node, "prev_value", prev_value_);
    return node;
}

TAstNode TChangeResource::GetAst(TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TChangeResource");
    AddReference(node, "pool_ref", pool_, ctx);
    AddValueField(node, "resource", id_);
    AddValueField(node, "delta", delta_);
    return node;
}

TAstNode TAddEffect::GetAst(TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TAddEffect");
    AddReference(node, "manager_ref", manager_, ctx);
    AddReference(node, "player_ref", player_, ctx);
    AddValueField(node, "condition", condition_);
    AddValueField(node, "value", value_);
    return node;
}

TAstNode TRemoveEffect::GetAst(TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TRemoveEffect");
    AddReference(node, "manager_ref", manager_, ctx);
    AddReference(node, "player_ref", player_, ctx);
    AddValueField(node, "condition", condition_);
    AddValueField(node, "value", value_);
    return node;
}

TAstNode TAddTask::GetAst(TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TAddTask");
    AddReference(node, "scheduler_ref", scheduler_, ctx);
    AddValueField(node, "task_id", task_id_);
    return node;
}

TAstNode TRemoveTask::GetAst(TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TRemoveTask");
    AddReference(node, "scheduler_ref", scheduler_, ctx);
    AddValueField(node, "task_id", task_id_);
    node.AddChild("task", GetTaskAst(task_, ctx, progress_index_));
    return node;
}

TAstNode TAdvanceTaskProgress::GetAst(TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TAdvanceTaskProgress");
    AddReference(node, "scheduler_ref", scheduler_, ctx);
    AddValueField(node, "task_id", task_id_);
    AddValueField(node, "prev_index", prev_index_);
    return node;
}

TAstNode TChangeCurrentPlayer::GetAst(TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TChangeCurrentPlayer");
    AddReference(node, "order_ref", order_, ctx);
    AddValueField(node, "prev_position", prev_position_);
    return node;
}

TAstNode TChangeRound::GetAst(TAstContext& ctx) const
{
    TAstNode node = TAstNode::MakeObject("TChangeRound");
    AddReference(node, "order_ref", order_, ctx);
    AddValueField(node, "prev_round", prev_round_);
    return node;
}

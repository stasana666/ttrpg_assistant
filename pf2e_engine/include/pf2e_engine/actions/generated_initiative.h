#pragma once

#include <pf2e_engine/actions/generated_action.h>

#include <memory>

void MoveActionEntityBeforeCurrentTurn(const std::shared_ptr<TActionContext>& context,
                                       const TGeneratedEntityView& entity);

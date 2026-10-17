#pragma once

#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/position.h>

#include <memory>

void MoveActionEntity(const std::shared_ptr<TActionContext>& context,
                      const TGeneratedEntityView& entity, TPosition expected_from,
                      TPosition destination);

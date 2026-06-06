#pragma once

#include <pf2e_engine/dsl/value.h>
#include <pf2e_engine/game_object_logic/game_object.h>

TGameObjectPtr ToGameObjectPtr(const TDslValue& v);

TDslValue::TList AsDslList(const TGameObjectPtr& obj);

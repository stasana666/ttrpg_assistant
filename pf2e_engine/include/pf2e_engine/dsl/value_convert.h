#pragma once

#include <pf2e_engine/dsl/value.h>
#include <pf2e_engine/game_object_logic/game_object.h>

// Lowers a TDslValue produced by the evaluator into the registry-side
// TGameObjectPtr. Lists are collapsed to TPlayerList or TWeaponList based on
// homogeneous element type; empty lists default to an empty TPlayerList.
// Throws on monostate/bool — those cannot live in the registry.
TGameObjectPtr ToGameObjectPtr(const TDslValue& v);

// Lifts a TPlayerList / TWeaponList living in TGameObjectPtr into a TDslList.
// Throws on non-list values.
TDslValue::TList AsDslList(const TGameObjectPtr& obj);

#pragma once

// Force registration of the built-in DSL properties and functions. Called
// from one well-known TU at program startup; subsequent calls are no-ops.
void EnsureDslBuiltinsRegistered();

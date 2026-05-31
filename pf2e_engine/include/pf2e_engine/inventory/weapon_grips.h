#pragma once

#include <vector>

class TWeapon;

// Allowed grip-counts for a weapon. Currently stub: every weapon is one-handed.
// Will expand when multi-grip weapons (two-handed, versatile) need to be expressed.
std::vector<int> Grips(const TWeapon& weapon);

bool ValidGrip(const TWeapon& weapon, int hand_count);

#pragma once

#include <vector>

class TWeapon;

std::vector<int> Grips(const TWeapon& weapon);

bool ValidGrip(const TWeapon& weapon, int hand_count);

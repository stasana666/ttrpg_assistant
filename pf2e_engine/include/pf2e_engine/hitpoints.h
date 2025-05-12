#pragma once

#include "observable.h"

class THitPoints : TObservable {
public:
    THitPoints(int maxHp);

    int GetCurrentHp() const;
    void ReduceHp(int damage);
    void RestoreHp(int heal);
    void SetTemporaryHp(int hp);

private:
    int MaxHp;
    int CurrentHp;
    int TemporaryHp;
};

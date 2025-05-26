#pragma once

class THitPoints {
public:
    explicit THitPoints(int max_hp);

    int GetCurrentHp() const;
    void ReduceHp(int damage);
    void RestoreHp(int heal);
    void SetTemporaryHp(int hp);

private:
    int max_hp;
    int current_hp;
    int temporary_hp;
};

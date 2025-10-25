#pragma once

class THitPoints {
public:
    explicit THitPoints(int max_hp);

    int GetCurrentHp() const;
    void ReduceHp(int damage);
    void RestoreHp(int heal);
    void SetTemporaryHp(int hp);
    int GetTemporaryHp() const;

private:
    int max_hp_;
    int current_hp_;
    int temporary_hp_;
};

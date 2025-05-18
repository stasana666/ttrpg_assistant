#pragma once

class TCreature;

class TArmourClass {
public:
    TArmourClass();

    void Bind(const TCreature* creature);

    int GetAc() const;

private:
    const TCreature* creature;
};

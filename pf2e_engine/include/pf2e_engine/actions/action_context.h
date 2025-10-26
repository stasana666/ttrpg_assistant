#pragma once

class TBattle;
class TTransformator;
class IRandomGenerator;
class TGameObjectRegister;

class TActionContext {
public:
    TGameObjectRegister* game_object_register;
    TBattle* battle;
    IRandomGenerator* dice_roller;
    TTransformator* transformator;
};

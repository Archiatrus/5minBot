#pragma once

#include "Common.h"
#include "MicroManager.h"

class CCBot;

class RangedManager: public MicroManager
{
public:

    RangedManager(CCBot & bot);
    void    executeMicro(const CUnits & targets);
    void    assignTargets(const CUnits & targets);
    int     getAttackPriority(const CUnit_ptr & target);
    const CUnit_ptr getTarget(const CUnit_ptr rangedUnit, const CUnits & targets);
};
